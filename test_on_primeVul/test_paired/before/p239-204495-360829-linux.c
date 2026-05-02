static int __io_sync_cancel(struct io_uring_task *tctx,
			    struct io_cancel_data *cd, int fd)
{
	struct io_ring_ctx *ctx = cd->ctx;

	/* fixed must be grabbed every time since we drop the uring_lock */
	if ((cd->flags & IORING_ASYNC_CANCEL_FD) &&
	    (cd->flags & IORING_ASYNC_CANCEL_FD_FIXED)) {
		unsigned long file_ptr;

		if (unlikely(fd > ctx->nr_user_files))
			return -EBADF;
		fd = array_index_nospec(fd, ctx->nr_user_files);
		file_ptr = io_fixed_file_slot(&ctx->file_table, fd)->file_ptr;
		cd->file = (struct file *) (file_ptr & FFS_MASK);
		if (!cd->file)
			return -EBADF;
	}

	return __io_async_cancel(cd, tctx, 0);
}