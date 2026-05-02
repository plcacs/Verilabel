p11_rpc_buffer_get_byte_array (p11_buffer *buf,
                               size_t *offset,
                               const unsigned char **data,
                               size_t *length)
{
	size_t off = *offset;
	uint32_t len;
	if (!p11_rpc_buffer_get_uint32 (buf, &off, &len))
		return false;
	if (len == 0xffffffff) {
		*offset = off;
		if (data)
			*data = NULL;
		if (length)
			*length = 0;
		return true;
	} else if (len >= 0x7fffffff) {
		p11_buffer_fail (buf);
		return false;
	}

	if (buf->len < len || *offset > buf->len - len) {
		p11_buffer_fail (buf);
		return false;
	}

	if (data)
		*data = (unsigned char *)buf->data + off;
	if (length)
		*length = len;
	*offset = off + len;

	return true;
}