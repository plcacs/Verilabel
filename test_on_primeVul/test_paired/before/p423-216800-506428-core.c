rpa_read_buffer(pool_t pool, const unsigned char **data,
		const unsigned char *end, unsigned char **buffer)
{
	const unsigned char *p = *data;
	unsigned int len;

	if (p > end)
		return 0;

	len = *p++;
	if (p + len > end)
		return 0;

	*buffer = p_malloc(pool, len);
	memcpy(*buffer, p, len);

	*data += 1 + len;

	return len;
}