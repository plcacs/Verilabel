static int io_read(struct io_kiocb *req, unsigned int issue_flags)
{
	struct iovec inline_vecs[UIO_FASTIOV], *iovec = inline_vecs;
	struct kiocb *kiocb = &req->rw.kiocb;
	struct iov_iter __iter, *iter = &__iter;
	struct io_async_rw *rw = req->async_data;
	ssize_t io_size, ret, ret2;
	bool force_nonblock = issue_flags & IO_URING_F_NONBLOCK;

	if (rw) {
		iter = &rw->iter;
		iovec = NULL;
	} else {
		ret = io_import_iovec(READ, req, &iovec, iter, !force_nonblock);
		if (ret < 0)
			return ret;
	}
	io_size = iov_iter_count(iter);
	req->result = io_size;

	/* Ensure we clear previously set non-block flag */
	if (!force_nonblock)
		kiocb->ki_flags &= ~IOCB_NOWAIT;
	else
		kiocb->ki_flags |= IOCB_NOWAIT;

	/* If the file doesn't support async, just async punt */
	if (force_nonblock && !io_file_supports_async(req, READ)) {
		ret = io_setup_async_rw(req, iovec, inline_vecs, iter, true);
		return ret ?: -EAGAIN;
	}

	ret = rw_verify_area(READ, req->file, io_kiocb_ppos(kiocb), io_size);
	if (unlikely(ret)) {
		kfree(iovec);
		return ret;
	}

	ret = io_iter_do_read(req, iter);

	if (ret == -EAGAIN || (req->flags & REQ_F_REISSUE)) {
		req->flags &= ~REQ_F_REISSUE;
		/* IOPOLL retry should happen for io-wq threads */
		if (!force_nonblock && !(req->ctx->flags & IORING_SETUP_IOPOLL))
			goto done;
		/* no retry on NONBLOCK nor RWF_NOWAIT */
		if (req->flags & REQ_F_NOWAIT)
			goto done;
		/* some cases will consume bytes even on error returns */
		iov_iter_revert(iter, io_size - iov_iter_count(iter));
		ret = 0;
	} else if (ret == -EIOCBQUEUED) {
		goto out_free;
	} else if (ret <= 0 || ret == io_size || !force_nonblock ||
		   (req->flags & REQ_F_NOWAIT) || !(req->flags & REQ_F_ISREG)) {
		/* read all, failed, already did sync or don't want to retry */
		goto done;
	}

	ret2 = io_setup_async_rw(req, iovec, inline_vecs, iter, true);
	if (ret2)
		return ret2;

	iovec = NULL;
	rw = req->async_data;
	/* now use our persistent iterator, if we aren't already */
	iter = &rw->iter;

	do {
		io_size -= ret;
		rw->bytes_done += ret;
		/* if we can retry, do so with the callbacks armed */
		if (!io_rw_should_retry(req)) {
			kiocb->ki_flags &= ~IOCB_WAITQ;
			return -EAGAIN;
		}

		/*
		 * Now retry read with the IOCB_WAITQ parts set in the iocb. If
		 * we get -EIOCBQUEUED, then we'll get a notification when the
		 * desired page gets unlocked. We can also get a partial read
		 * here, and if we do, then just retry at the new offset.
		 */
		ret = io_iter_do_read(req, iter);
		if (ret == -EIOCBQUEUED)
			return 0;
		/* we got some bytes, but not all. retry. */
		kiocb->ki_flags &= ~IOCB_WAITQ;
	} while (ret > 0 && ret < io_size);
done:
	kiocb_done(kiocb, ret, issue_flags);
out_free:
	/* it's faster to check here then delegate to kfree */
	if (iovec)
		kfree(iovec);
	return 0;
}