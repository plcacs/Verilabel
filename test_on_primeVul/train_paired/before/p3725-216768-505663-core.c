static void snippet_add_content(struct snippet_context *ctx,
				struct snippet_data *target,
				const unsigned char *data, size_t size,
				size_t *count_r)
{
	i_assert(target != NULL);
	if (size >= 3 &&
	     ((data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) ||
	      (data[0] == 0xBF && data[1] == 0xBB && data[2] == 0xEF))) {
		*count_r = 3;
		return;
	}
	if (data[0] == '\0') {
		/* skip NULs without increasing snippet size */
		return;
	}
	if (i_isspace(*data)) {
		/* skip any leading whitespace */
		if (str_len(target->snippet) > 1)
			ctx->add_whitespace = TRUE;
		if (data[0] == '\n')
			ctx->state = SNIPPET_STATE_NEWLINE;
		return;
	}
	if (ctx->add_whitespace) {
		str_append_c(target->snippet, ' ');
		ctx->add_whitespace = FALSE;
		if (target->chars_left-- == 0)
			return;
	}
	if (target->chars_left == 0)
		return;
	target->chars_left--;
	*count_r = uni_utf8_char_bytes(data[0]);
	i_assert(*count_r <= size);
	str_append_data(target->snippet, data, *count_r);
}