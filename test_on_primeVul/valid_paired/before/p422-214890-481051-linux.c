static unsigned int xdr_set_page_base(struct xdr_stream *xdr,
				      unsigned int base, unsigned int len)
{
	unsigned int pgnr;
	unsigned int maxlen;
	unsigned int pgoff;
	unsigned int pgend;
	void *kaddr;

	maxlen = xdr->buf->page_len;
	if (base >= maxlen) {
		base = maxlen;
		maxlen = 0;
	} else
		maxlen -= base;
	if (len > maxlen)
		len = maxlen;

	xdr_stream_page_set_pos(xdr, base);
	base += xdr->buf->page_base;

	pgnr = base >> PAGE_SHIFT;
	xdr->page_ptr = &xdr->buf->pages[pgnr];
	kaddr = page_address(*xdr->page_ptr);

	pgoff = base & ~PAGE_MASK;
	xdr->p = (__be32*)(kaddr + pgoff);

	pgend = pgoff + len;
	if (pgend > PAGE_SIZE)
		pgend = PAGE_SIZE;
	xdr->end = (__be32*)(kaddr + pgend);
	xdr->iov = NULL;
	return len;
}