static void
bfq_idle_slice_timer_body(struct bfq_data *bfqd, struct bfq_queue *bfqq)
{
	enum bfqq_expiration reason;
	unsigned long flags;

	spin_lock_irqsave(&bfqd->lock, flags);

	/*
	 * Considering that bfqq may be in race, we should firstly check
	 * whether bfqq is in service before doing something on it. If
	 * the bfqq in race is not in service, it has already been expired
	 * through __bfq_bfqq_expire func and its wait_request flags has
	 * been cleared in __bfq_bfqd_reset_in_service func.
	 */
	if (bfqq != bfqd->in_service_queue) {
		spin_unlock_irqrestore(&bfqd->lock, flags);
		return;
	}

	bfq_clear_bfqq_wait_request(bfqq);

	if (bfq_bfqq_budget_timeout(bfqq))
		/*
		 * Also here the queue can be safely expired
		 * for budget timeout without wasting
		 * guarantees
		 */
		reason = BFQQE_BUDGET_TIMEOUT;
	else if (bfqq->queued[0] == 0 && bfqq->queued[1] == 0)
		/*
		 * The queue may not be empty upon timer expiration,
		 * because we may not disable the timer when the
		 * first request of the in-service queue arrives
		 * during disk idling.
		 */
		reason = BFQQE_TOO_IDLE;
	else
		goto schedule_dispatch;

	bfq_bfqq_expire(bfqd, bfqq, true, reason);

schedule_dispatch:
	spin_unlock_irqrestore(&bfqd->lock, flags);
	bfq_schedule_dispatch(bfqd);