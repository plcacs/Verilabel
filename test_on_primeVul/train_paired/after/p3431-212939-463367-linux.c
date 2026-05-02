void blk_cleanup_queue(struct request_queue *q)
{
	/* mark @q DYING, no new request or merges will be allowed afterwards */
	mutex_lock(&q->sysfs_lock);
	blk_set_queue_dying(q);

	blk_queue_flag_set(QUEUE_FLAG_NOMERGES, q);
	blk_queue_flag_set(QUEUE_FLAG_NOXMERGES, q);
	blk_queue_flag_set(QUEUE_FLAG_DYING, q);
	mutex_unlock(&q->sysfs_lock);

	/*
	 * Drain all requests queued before DYING marking. Set DEAD flag to
	 * prevent that q->request_fn() gets invoked after draining finished.
	 */
	blk_freeze_queue(q);

	rq_qos_exit(q);

	blk_queue_flag_set(QUEUE_FLAG_DEAD, q);

	/* for synchronous bio-based driver finish in-flight integrity i/o */
	blk_flush_integrity();

	/* @q won't process any more request, flush async actions */
	del_timer_sync(&q->backing_dev_info->laptop_mode_wb_timer);
	blk_sync_queue(q);

	if (queue_is_mq(q))
		blk_mq_exit_queue(q);

	/*
	 * In theory, request pool of sched_tags belongs to request queue.
	 * However, the current implementation requires tag_set for freeing
	 * requests, so free the pool now.
	 *
	 * Queue has become frozen, there can't be any in-queue requests, so
	 * it is safe to free requests now.
	 */
	mutex_lock(&q->sysfs_lock);
	if (q->elevator)
		blk_mq_sched_free_requests(q);
	mutex_unlock(&q->sysfs_lock);

	percpu_ref_exit(&q->q_usage_counter);

	/* @q is and will stay empty, shutdown and put */
	blk_put_queue(q);
}