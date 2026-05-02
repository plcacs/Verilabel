soup_filter_input_stream_read_until (SoupFilterInputStream  *fstream,
				     void                   *buffer,
				     gsize                   length,
				     const void             *boundary,
				     gsize                   boundary_length,
				     gboolean                blocking,
				     gboolean                include_boundary,
				     gboolean               *got_boundary,
				     GCancellable           *cancellable,
				     GError                **error)
{
	gssize nread, read_length;
	guint8 *p, *buf, *end;
	gboolean eof = FALSE;
	GError *my_error = NULL;

	g_return_val_if_fail (SOUP_IS_FILTER_INPUT_STREAM (fstream), -1);
	g_return_val_if_fail (!include_boundary || (boundary_length < length), -1);

	*got_boundary = FALSE;
	fstream->priv->need_more = FALSE;

	if (!fstream->priv->buf || fstream->priv->buf->len < boundary_length) {
		guint prev_len;

	fill_buffer:
		if (!fstream->priv->buf)
			fstream->priv->buf = g_byte_array_new ();
		prev_len = fstream->priv->buf->len;
		g_byte_array_set_size (fstream->priv->buf, length);
		buf = fstream->priv->buf->data;

		fstream->priv->in_read_until = TRUE;
		nread = g_pollable_stream_read (G_INPUT_STREAM (fstream),
						buf + prev_len, length - prev_len,
						blocking,
						cancellable, &my_error);
		fstream->priv->in_read_until = FALSE;
		if (nread <= 0) {
			if (prev_len)
				fstream->priv->buf->len = prev_len;
			else {
				g_byte_array_free (fstream->priv->buf, TRUE);
				fstream->priv->buf = NULL;
			}

			if (nread == 0 && prev_len)
				eof = TRUE;
			else {
				if (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
					fstream->priv->need_more = TRUE;
				if (my_error)
					g_propagate_error (error, my_error);

				return nread;
			}

			if (my_error)
				g_propagate_error (error, my_error);
		} else
			fstream->priv->buf->len = prev_len + nread;
	} else
		buf = fstream->priv->buf->data;

	/* Scan for the boundary within the range we can possibly return. */
	if (include_boundary)
		end = buf + MIN (fstream->priv->buf->len, length) - boundary_length;
	else
		end = buf + MIN (fstream->priv->buf->len - boundary_length, length);
	for (p = buf; p <= end; p++) {
		if (*p == *(guint8*)boundary &&
		    !memcmp (p, boundary, boundary_length)) {
			if (include_boundary)
				p += boundary_length;
			*got_boundary = TRUE;
			break;
		}
	}

	if (!*got_boundary && fstream->priv->buf->len < length && !eof)
		goto fill_buffer;

	if (eof && !*got_boundary)
		read_length = MIN (fstream->priv->buf->len, length);
	else
		read_length = p - buf;
	return read_from_buf (fstream, buffer, read_length);
}