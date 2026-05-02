translate_input(int forward_translation, char *table_name) {
	char charbuf[BUFSIZE];
	uint8_t *outputbuf;
	size_t outlen;
	widechar inbuf[BUFSIZE];
	widechar transbuf[BUFSIZE];
	int inlen;
	int translen;
	int k;
	int ch = 0;
	int result;
	while (1) {
		translen = BUFSIZE;
		k = 0;
		while ((ch = fgetc(input)) != '\n' && ch != EOF && k < BUFSIZE - 1)
			charbuf[k++] = ch;
		if (ch == EOF && k == 0) break;
		charbuf[k] = 0;
		inlen = _lou_extParseChars(charbuf, inbuf);
		if (forward_translation)
			result = lou_translateString(
					table_name, inbuf, &inlen, transbuf, &translen, NULL, NULL, 0);
		else
			result = lou_backTranslateString(
					table_name, inbuf, &inlen, transbuf, &translen, NULL, NULL, 0);
		if (!result) break;
#ifdef WIDECHARS_ARE_UCS4
		outputbuf = u32_to_u8(transbuf, translen, NULL, &outlen);
#else
		outputbuf = u16_to_u8(transbuf, translen, NULL, &outlen);
#endif
		printf(ch == EOF ? "%.*s" : "%.*s\n", (int)outlen, outputbuf);
		free(outputbuf);
	}
	lou_free();
}