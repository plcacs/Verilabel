static int convert_case_filter(int c, void *void_data)
{
	struct convert_case_data *data = (struct convert_case_data *) void_data;
	unsigned out[3];
	unsigned len, i;

	/* Handle invalid characters early, as we assign special meaning to
	 * codepoints above 0xffffff. */
	if (UNEXPECTED((unsigned) c > 0xffffff)) {
		(*data->next_filter->filter_function)(c, data->next_filter);
		return 0;
	}

	switch (data->case_mode) {
		case PHP_UNICODE_CASE_UPPER_SIMPLE:
			out[0] = php_unicode_toupper_simple(c, data->no_encoding);
			len = 1;
			break;

		case PHP_UNICODE_CASE_UPPER:
			len = php_unicode_toupper_full(c, data->no_encoding, out);
			break;

		case PHP_UNICODE_CASE_LOWER_SIMPLE:
			out[0] = php_unicode_tolower_simple(c, data->no_encoding);
			len = 1;
			break;

		case PHP_UNICODE_CASE_LOWER:
			len = php_unicode_tolower_full(c, data->no_encoding, out);
			break;

		case PHP_UNICODE_CASE_FOLD:
			len = php_unicode_tofold_full(c, data->no_encoding, out);
			break;

		case PHP_UNICODE_CASE_FOLD_SIMPLE:
			out[0] = php_unicode_tofold_simple(c, data->no_encoding);
			len = 1;
			break;

		case PHP_UNICODE_CASE_TITLE_SIMPLE:
		case PHP_UNICODE_CASE_TITLE:
		{
			if (data->title_mode) {
				if (data->case_mode == PHP_UNICODE_CASE_TITLE_SIMPLE) {
					out[0] = php_unicode_tolower_simple(c, data->no_encoding);
					len = 1;
				} else {
					len = php_unicode_tolower_full(c, data->no_encoding, out);
				}
			} else {
				if (data->case_mode == PHP_UNICODE_CASE_TITLE_SIMPLE) {
					out[0] = php_unicode_totitle_simple(c, data->no_encoding);
					len = 1;
				} else {
					len = php_unicode_totitle_full(c, data->no_encoding, out);
				}
			}
			if (!php_unicode_is_case_ignorable(c)) {
				data->title_mode = php_unicode_is_cased(c);
			}
			break;
		}
		EMPTY_SWITCH_DEFAULT_CASE()
	}

	for (i = 0; i < len; i++) {
		(*data->next_filter->filter_function)(out[i], data->next_filter);
	}
	return 0;
}