static void io_async_task_func(struct callback_head *cb)
{
	struct io_kiocb *req = container_of(cb, struct io_kiocb, task_work);
	struct async_poll *apoll = req->apoll;
	struct io_ring_ctx *ctx = req->ctx;

	trace_io_uring_task_run(req->ctx, req->opcode, req->user_data);

	if (io_poll_rewait(req, &apoll->poll)) {
		spin_unlock_irq(&ctx->completion_lock);
		return;
	}

	/* If req is still hashed, it cannot have been canceled. Don't check. */
	if (hash_hashed(&req->hash_node))
		hash_del(&req->hash_node);

	io_poll_remove_double(req, apoll->double_poll);
	spin_unlock_irq(&ctx->completion_lock);

	if (!READ_ONCE(apoll->poll.canceled))
		__io_req_task_submit(req);
	else
		__io_req_task_cancel(req, -ECANCELED);

	kfree(apoll->double_poll);
	kfree(apoll);
}