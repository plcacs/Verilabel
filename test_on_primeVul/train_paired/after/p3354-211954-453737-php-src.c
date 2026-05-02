static int php_iconv_stream_filter_append_bucket(
		php_iconv_stream_filter *self,
		php_stream *stream, php_stream_filter *filter,
		php_stream_bucket_brigade *buckets_out,
		const char *ps, size_t buf_len, size_t *consumed,
		int persistent TSRMLS_DC)
{
	php_stream_bucket *new_bucket;
	char *out_buf = NULL;
	size_t out_buf_size;
	char *pd, *pt;
	size_t ocnt, prev_ocnt, icnt, tcnt;
	size_t initial_out_buf_size;

	if (ps == NULL) {
		initial_out_buf_size = 64;
		icnt = 1;
	} else {
		initial_out_buf_size = buf_len;
		icnt = buf_len;
	}

	out_buf_size = ocnt = prev_ocnt = initial_out_buf_size;
	if (NULL == (out_buf = pemalloc(out_buf_size, persistent))) {
		return FAILURE;
	}

	pd = out_buf;

	if (self->stub_len > 0) {
		pt = self->stub;
		tcnt = self->stub_len;

		while (tcnt > 0) {
			if (iconv(self->cd, &pt, &tcnt, &pd, &ocnt) == (size_t)-1) {
#if ICONV_SUPPORTS_ERRNO
				switch (errno) {
					case EILSEQ:
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): invalid multibyte sequence", self->from_charset, self->to_charset);
						goto out_failure;

					case EINVAL:
						if (ps != NULL) {
							if (icnt > 0) {
								if (self->stub_len >= sizeof(self->stub)) {
									php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): insufficient buffer", self->from_charset, self->to_charset);
									goto out_failure;
								}
								self->stub[self->stub_len++] = *(ps++);
								icnt--;
								pt = self->stub;
								tcnt = self->stub_len;
							} else {
								tcnt = 0;
								break;
							}
						} else {
						    php_error_docref(NULL, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): invalid multibyte sequence", self->from_charset, self->to_charset);
						    goto out_failure;
						}
						break;

					case E2BIG: {
						char *new_out_buf;
						size_t new_out_buf_size;

						new_out_buf_size = out_buf_size << 1;

						if (new_out_buf_size < out_buf_size) {
							/* whoa! no bigger buckets are sold anywhere... */
							if (NULL == (new_bucket = php_stream_bucket_new(stream, out_buf, (out_buf_size - ocnt), 1, persistent TSRMLS_CC))) {
								goto out_failure;
							}

							php_stream_bucket_append(buckets_out, new_bucket TSRMLS_CC);

							out_buf_size = ocnt = initial_out_buf_size;
							if (NULL == (out_buf = pemalloc(out_buf_size, persistent))) {
								return FAILURE;
							}
							pd = out_buf;
						} else {
							if (NULL == (new_out_buf = perealloc(out_buf, new_out_buf_size, persistent))) {
								if (NULL == (new_bucket = php_stream_bucket_new(stream, out_buf, (out_buf_size - ocnt), 1, persistent TSRMLS_CC))) {
									goto out_failure;
								}

								php_stream_bucket_append(buckets_out, new_bucket TSRMLS_CC);
								return FAILURE;
							}
							pd = new_out_buf + (pd - out_buf);
							ocnt += (new_out_buf_size - out_buf_size);
							out_buf = new_out_buf;
							out_buf_size = new_out_buf_size;
						}
					} break;

					default:
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): unknown error", self->from_charset, self->to_charset);
						goto out_failure;
				}
#else
				if (ocnt == prev_ocnt) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): unknown error", self->from_charset, self->to_charset);
					goto out_failure;
				}
#endif
			}
			prev_ocnt = ocnt;
		}
		memmove(self->stub, pt, tcnt);
		self->stub_len = tcnt;
	}

	while (icnt > 0) {
		if ((ps == NULL ? iconv(self->cd, NULL, NULL, &pd, &ocnt):
					iconv(self->cd, (char **)&ps, &icnt, &pd, &ocnt)) == (size_t)-1) {
#if ICONV_SUPPORTS_ERRNO
			switch (errno) {
				case EILSEQ:
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): invalid multibyte sequence", self->from_charset, self->to_charset);
					goto out_failure;

				case EINVAL:
					if (ps != NULL) {
						if (icnt > sizeof(self->stub)) {
							php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): insufficient buffer", self->from_charset, self->to_charset);
							goto out_failure;
						}
						memcpy(self->stub, ps, icnt);
						self->stub_len = icnt;
						ps += icnt;
						icnt = 0;
					} else {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): unexpected octet values", self->from_charset, self->to_charset);
						goto out_failure;
					}
					break;

				case E2BIG: {
					char *new_out_buf;
					size_t new_out_buf_size;

					new_out_buf_size = out_buf_size << 1;

					if (new_out_buf_size < out_buf_size) {
						/* whoa! no bigger buckets are sold anywhere... */
						if (NULL == (new_bucket = php_stream_bucket_new(stream, out_buf, (out_buf_size - ocnt), 1, persistent TSRMLS_CC))) {
							goto out_failure;
						}

						php_stream_bucket_append(buckets_out, new_bucket TSRMLS_CC);

						out_buf_size = ocnt = initial_out_buf_size;
						if (NULL == (out_buf = pemalloc(out_buf_size, persistent))) {
							return FAILURE;
						}
						pd = out_buf;
					} else {
						if (NULL == (new_out_buf = perealloc(out_buf, new_out_buf_size, persistent))) {
							if (NULL == (new_bucket = php_stream_bucket_new(stream, out_buf, (out_buf_size - ocnt), 1, persistent TSRMLS_CC))) {
								goto out_failure;
							}

							php_stream_bucket_append(buckets_out, new_bucket TSRMLS_CC);
							return FAILURE;
						}
						pd = new_out_buf + (pd - out_buf);
						ocnt += (new_out_buf_size - out_buf_size);
						out_buf = new_out_buf;
						out_buf_size = new_out_buf_size;
					}
				} break;

				default:
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): unknown error", self->from_charset, self->to_charset);
					goto out_failure;
			}
#else
			if (ocnt == prev_ocnt) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "iconv stream filter (\"%s\"=>\"%s\"): unknown error", self->from_charset, self->to_charset);
				goto out_failure;
			}
#endif
		} else {
			if (ps == NULL) {
				break;
			}
		}
		prev_ocnt = ocnt;
	}

	if (out_buf_size - ocnt > 0) {
		if (NULL == (new_bucket = php_stream_bucket_new(stream, out_buf, (out_buf_size - ocnt), 1, persistent TSRMLS_CC))) {
			goto out_failure;
		}
		php_stream_bucket_append(buckets_out, new_bucket TSRMLS_CC);
	} else {
		pefree(out_buf, persistent);
	}
	*consumed += buf_len - icnt;

	return SUCCESS;

out_failure:
	pefree(out_buf, persistent);
	return FAILURE;
}