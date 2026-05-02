static int io_add_buffers(struct io_provide_buf *pbuf, struct io_buffer **head)
{
	struct io_buffer *buf;
	u64 addr = pbuf->addr;
	int i, bid = pbuf->bid;

	for (i = 0; i < pbuf->nbufs; i++) {
		buf = kmalloc(sizeof(*buf), GFP_KERNEL);
		if (!buf)
			break;

		buf->addr = addr;
		buf->len = pbuf->len;
		buf->bid = bid;
		addr += pbuf->len;
		bid++;
		if (!*head) {
			INIT_LIST_HEAD(&buf->list);
			*head = buf;
		} else {
			list_add_tail(&buf->list, &(*head)->list);
		}
	}

	return i ? i : -ENOMEM;
}