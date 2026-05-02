static size_t php_bz2iop_read(php_stream *stream, char *buf, size_t count TSRMLS_DC)
{
	struct php_bz2_stream_data_t *self = (struct php_bz2_stream_data_t *) stream->abstract;
	int bz2_ret;

	bz2_ret = BZ2_bzread(self->bz_file, buf, count);

	if (bz2_ret < 0) {
		stream->eof = 1;
		return -1;
	}
	if (bz2_ret == 0) {
		stream->eof = 1;
	}

	return (size_t)bz2_ret;
}