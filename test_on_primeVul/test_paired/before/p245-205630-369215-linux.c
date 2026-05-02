static int io_rw_init_file(struct io_kiocb *req, fmode_t mode)
{
	struct kiocb *kiocb = &req->rw.kiocb;
	struct io_ring_ctx *ctx = req->ctx;
	struct file *file = req->file;
	int ret;

	if (unlikely(!file || !(file->f_mode & mode)))
		return -EBADF;

	if (!io_req_ffs_set(req))
		req->flags |= io_file_get_flags(file) << REQ_F_SUPPORT_NOWAIT_BIT;

	kiocb->ki_flags = iocb_flags(file);
	ret = kiocb_set_rw_flags(kiocb, req->rw.flags);
	if (unlikely(ret))
		return ret;

	/*
	 * If the file is marked O_NONBLOCK, still allow retry for it if it
	 * supports async. Otherwise it's impossible to use O_NONBLOCK files
	 * reliably. If not, or it IOCB_NOWAIT is set, don't retry.
	 */
	if ((kiocb->ki_flags & IOCB_NOWAIT) ||
	    ((file->f_flags & O_NONBLOCK) && !io_file_supports_nowait(req)))
		req->flags |= REQ_F_NOWAIT;

	if (ctx->flags & IORING_SETUP_IOPOLL) {
		if (!(kiocb->ki_flags & IOCB_DIRECT) || !file->f_op->iopoll)
			return -EOPNOTSUPP;

		kiocb->ki_flags |= IOCB_HIPRI | IOCB_ALLOC_CACHE;
		kiocb->ki_complete = io_complete_rw_iopoll;
		req->iopoll_completed = 0;
	} else {
		if (kiocb->ki_flags & IOCB_HIPRI)
			return -EINVAL;
		kiocb->ki_complete = io_complete_rw;
	}

	return 0;
}