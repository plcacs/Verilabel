static int multipart_buffer_headers(multipart_buffer *self, zend_llist *header TSRMLS_DC)
{
	char *line;
	mime_header_entry entry = {0};
	smart_str buf_value = {0};
	char *key = NULL;

	/* didn't find boundary, abort */
	if (!find_boundary(self, self->boundary TSRMLS_CC)) {
		return 0;
	}

	/* get lines of text, or CRLF_CRLF */

	while( (line = get_line(self TSRMLS_CC)) && strlen(line) > 0 )
	{
		/* add header to table */
		char *value = NULL;

		if (php_rfc1867_encoding_translation(TSRMLS_C)) {
			self->input_encoding = zend_multibyte_encoding_detector((unsigned char *)line, strlen(line), self->detect_order, self->detect_order_size TSRMLS_CC);
		}

		/* space in the beginning means same header */
		if (!isspace(line[0])) {
			value = strchr(line, ':');
		}

		if (value) {
			if(buf_value.c && key) {
				/* new entry, add the old one to the list */
				smart_str_0(&buf_value);
				entry.key = key;
				entry.value = buf_value.c;
				zend_llist_add_element(header, &entry);
				buf_value.c = NULL;
				key = NULL;
			}

			*value = '\0';
			do { value++; } while(isspace(*value));

			key = estrdup(line);
			smart_str_appends(&buf_value, value);
		} else if (buf_value.c) { /* If no ':' on the line, add to previous line */
			smart_str_appends(&buf_value, line);
		} else {
			continue;
		}
	}
	if(buf_value.c && key) {
		/* add the last one to the list */
		smart_str_0(&buf_value);
		entry.key = key;
		entry.value = buf_value.c;
		zend_llist_add_element(header, &entry);
	}

	return 1;
}