PHP_XML_API zend_string *xml_utf8_encode(const char *s, size_t len, const XML_Char *encoding)
{
	size_t pos = len;
	zend_string *str;
	unsigned int c;
	unsigned short (*encoder)(unsigned char) = NULL;
	xml_encoding *enc = xml_get_encoding(encoding);

	if (enc) {
		encoder = enc->encoding_function;
	} else {
		/* If the target encoding was unknown, fail */
		return NULL;
	}
	if (encoder == NULL) {
		/* If no encoder function was specified, return the data as-is.
		 */
		str = zend_string_init(s, len, 0);
		return str;
	}
	/* This is the theoretical max (will never get beyond len * 2 as long
	 * as we are converting from single-byte characters, though) */
	str = zend_string_alloc(len * 4, 0);
	ZSTR_LEN(str) = 0;
	while (pos > 0) {
		c = encoder ? encoder((unsigned char)(*s)) : (unsigned short)(*s);
		if (c < 0x80) {
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (char) c;
		} else if (c < 0x800) {
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0xc0 | (c >> 6));
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0x80 | (c & 0x3f));
		} else if (c < 0x10000) {
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0xe0 | (c >> 12));
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0xc0 | ((c >> 6) & 0x3f));
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0x80 | (c & 0x3f));
		} else if (c < 0x200000) {
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0xf0 | (c >> 18));
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0xe0 | ((c >> 12) & 0x3f));
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0xc0 | ((c >> 6) & 0x3f));
			ZSTR_VAL(str)[ZSTR_LEN(str)++] = (0x80 | (c & 0x3f));
		}
		pos--;
		s++;
	}
	ZSTR_VAL(str)[ZSTR_LEN(str)] = '\0';
	str = zend_string_truncate(str, ZSTR_LEN(str), 0);
	return str;
}