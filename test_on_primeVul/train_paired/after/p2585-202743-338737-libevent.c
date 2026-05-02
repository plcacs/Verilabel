evbuffer_expand(struct evbuffer *buf, size_t datlen)
{
	size_t used = buf->misalign + buf->off;
	size_t need;

	assert(buf->totallen >= used);

	/* If we can fit all the data, then we don't have to do anything */
	if (buf->totallen - used >= datlen)
		return (0);
	/* If we would need to overflow to fit this much data, we can't
	 * do anything. */
	if (datlen > SIZE_MAX - buf->off)
		return (-1);

	/*
	 * If the misalignment fulfills our data needs, we just force an
	 * alignment to happen.  Afterwards, we have enough space.
	 */
	if (buf->totallen - buf->off >= datlen) {
		evbuffer_align(buf);
	} else {
		void *newbuf;
		size_t length = buf->totallen;
		size_t need = buf->off + datlen;

		if (length < 256)
			length = 256;
		if (need < SIZE_MAX / 2) {
			while (length < need) {
				length <<= 1;
			}
		} else {
			length = need;
		}

		if (buf->orig_buffer != buf->buffer)
			evbuffer_align(buf);
		if ((newbuf = realloc(buf->buffer, length)) == NULL)
			return (-1);

		buf->orig_buffer = buf->buffer = newbuf;
		buf->totallen = length;
	}

	return (0);
}