int digest_generic_verify(struct digest *d, const unsigned char *md)
{
	int ret;
	int len = digest_length(d);
	unsigned char *tmp;

	tmp = xmalloc(len);

	ret = digest_final(d, tmp);
	if (ret)
		goto end;

	ret = memcmp(md, tmp, len);
	ret = ret ? -EINVAL : 0;
end:
	free(tmp);
	return ret;
}