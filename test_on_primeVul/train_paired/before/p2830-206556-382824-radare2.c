R_API int r_egg_compile(REgg *egg) {
	r_buf_seek (egg->src, 0, R_BUF_SET);
	char b;
	int r = r_buf_read (egg->src, (ut8 *)&b, sizeof (b));
	if (r != sizeof (b) || !egg->remit) {
		return true;
	}
	// only emit begin if code is found
	r_egg_lang_init (egg);
	for (; b; ) {
		r_egg_lang_parsechar (egg, b);
		int r = r_buf_read (egg->src, (ut8 *)&b, sizeof (b));
		if (r != sizeof (b)) {
			break;
		}
		// XXX: some parse fail errors are false positives :(
	}
	if (egg->context>0) {
		eprintf ("ERROR: expected '}' at the end of the file. %d left\n", egg->context);
		return false;
	}
	// TODO: handle errors here
	return true;
}