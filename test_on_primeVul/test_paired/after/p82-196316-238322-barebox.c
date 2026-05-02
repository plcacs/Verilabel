int digest_generic_verify(struct digest *d, const unsigned char *md)
{
	int ret;
	int len = digest_length(d);
	unsigned char *tmp;

	tmp = xmalloc(len);

	ret = digest_final(d, tmp);
	if (ret)
		goto end;

	if (crypto_memneq(md, tmp, len))
		ret = -EINVAL;
	else
		ret = 0;
end:
	free(tmp);
	return ret;
}