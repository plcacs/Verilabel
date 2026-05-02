static ssize_t aio_poll(struct aio_kiocb *aiocb, const struct iocb *iocb)
{
	struct kioctx *ctx = aiocb->ki_ctx;
	struct poll_iocb *req = &aiocb->poll;
	struct aio_poll_table apt;
	__poll_t mask;

	/* reject any unknown events outside the normal event mask. */
	if ((u16)iocb->aio_buf != iocb->aio_buf)
		return -EINVAL;
	/* reject fields that are not defined for poll */
	if (iocb->aio_offset || iocb->aio_nbytes || iocb->aio_rw_flags)
		return -EINVAL;

	INIT_WORK(&req->work, aio_poll_complete_work);
	req->events = demangle_poll(iocb->aio_buf) | EPOLLERR | EPOLLHUP;
	req->file = fget(iocb->aio_fildes);
	if (unlikely(!req->file))
		return -EBADF;

	req->head = NULL;
	req->woken = false;
	req->cancelled = false;

	apt.pt._qproc = aio_poll_queue_proc;
	apt.pt._key = req->events;
	apt.iocb = aiocb;
	apt.error = -EINVAL; /* same as no support for IOCB_CMD_POLL */

	/* initialized the list so that we can do list_empty checks */
	INIT_LIST_HEAD(&req->wait.entry);
	init_waitqueue_func_entry(&req->wait, aio_poll_wake);

	/* one for removal from waitqueue, one for this function */
	refcount_set(&aiocb->ki_refcnt, 2);

	mask = vfs_poll(req->file, &apt.pt) & req->events;
	if (unlikely(!req->head)) {
		/* we did not manage to set up a waitqueue, done */
		goto out;
	}

	spin_lock_irq(&ctx->ctx_lock);
	spin_lock(&req->head->lock);
	if (req->woken) {
		/* wake_up context handles the rest */
		mask = 0;
		apt.error = 0;
	} else if (mask || apt.error) {
		/* if we get an error or a mask we are done */
		WARN_ON_ONCE(list_empty(&req->wait.entry));
		list_del_init(&req->wait.entry);
	} else {
		/* actually waiting for an event */
		list_add_tail(&aiocb->ki_list, &ctx->active_reqs);
		aiocb->ki_cancel = aio_poll_cancel;
	}
	spin_unlock(&req->head->lock);
	spin_unlock_irq(&ctx->ctx_lock);

out:
	if (unlikely(apt.error)) {
		fput(req->file);
		return apt.error;
	}

	if (mask)
		aio_poll_complete(aiocb, mask);
	iocb_put(aiocb);
	return 0;
}