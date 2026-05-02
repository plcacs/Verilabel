static size_t push_pipe(struct iov_iter *i, size_t size,
			int *iter_headp, size_t *offp)
{
	struct pipe_inode_info *pipe = i->pipe;
	unsigned int p_tail = pipe->tail;
	unsigned int p_mask = pipe->ring_size - 1;
	unsigned int iter_head;
	size_t off;
	ssize_t left;

	if (unlikely(size > i->count))
		size = i->count;
	if (unlikely(!size))
		return 0;

	left = size;
	data_start(i, &iter_head, &off);
	*iter_headp = iter_head;
	*offp = off;
	if (off) {
		left -= PAGE_SIZE - off;
		if (left <= 0) {
			pipe->bufs[iter_head & p_mask].len += size;
			return size;
		}
		pipe->bufs[iter_head & p_mask].len = PAGE_SIZE;
		iter_head++;
	}
	while (!pipe_full(iter_head, p_tail, pipe->max_usage)) {
		struct pipe_buffer *buf = &pipe->bufs[iter_head & p_mask];
		struct page *page = alloc_page(GFP_USER);
		if (!page)
			break;

		buf->ops = &default_pipe_buf_ops;
		buf->flags = 0;
		buf->page = page;
		buf->offset = 0;
		buf->len = min_t(ssize_t, left, PAGE_SIZE);
		left -= buf->len;
		iter_head++;
		pipe->head = iter_head;

		if (left == 0)
			return size;
	}
	return size - left;
}