static size_t copy_page_to_iter_pipe(struct page *page, size_t offset, size_t bytes,
			 struct iov_iter *i)
{
	struct pipe_inode_info *pipe = i->pipe;
	struct pipe_buffer *buf;
	unsigned int p_tail = pipe->tail;
	unsigned int p_mask = pipe->ring_size - 1;
	unsigned int i_head = i->head;
	size_t off;

	if (unlikely(bytes > i->count))
		bytes = i->count;

	if (unlikely(!bytes))
		return 0;

	if (!sanity(i))
		return 0;

	off = i->iov_offset;
	buf = &pipe->bufs[i_head & p_mask];
	if (off) {
		if (offset == off && buf->page == page) {
			/* merge with the last one */
			buf->len += bytes;
			i->iov_offset += bytes;
			goto out;
		}
		i_head++;
		buf = &pipe->bufs[i_head & p_mask];
	}
	if (pipe_full(i_head, p_tail, pipe->max_usage))
		return 0;

	buf->ops = &page_cache_pipe_buf_ops;
	buf->flags = 0;
	get_page(page);
	buf->page = page;
	buf->offset = offset;
	buf->len = bytes;

	pipe->head = i_head + 1;
	i->iov_offset = offset + bytes;
	i->head = i_head;
out:
	i->count -= bytes;
	return bytes;
}