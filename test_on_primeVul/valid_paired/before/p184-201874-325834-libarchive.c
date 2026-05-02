archive_string_append_from_wcs(struct archive_string *as,
    const wchar_t *w, size_t len)
{
	/* We cannot use the standard wcstombs() here because it
	 * cannot tell us how big the output buffer should be.  So
	 * I've built a loop around wcrtomb() or wctomb() that
	 * converts a character at a time and resizes the string as
	 * needed.  We prefer wcrtomb() when it's available because
	 * it's thread-safe. */
	int n, ret_val = 0;
	char *p;
	char *end;
#if HAVE_WCRTOMB
	mbstate_t shift_state;

	memset(&shift_state, 0, sizeof(shift_state));
#else
	/* Clear the shift state before starting. */
	wctomb(NULL, L'\0');
#endif
	/*
	 * Allocate buffer for MBS.
	 * We need this allocation here since it is possible that
	 * as->s is still NULL.
	 */
	if (archive_string_ensure(as, as->length + len + 1) == NULL)
		return (-1);

	p = as->s + as->length;
	end = as->s + as->buffer_length - MB_CUR_MAX -1;
	while (*w != L'\0' && len > 0) {
		if (p >= end) {
			as->length = p - as->s;
			as->s[as->length] = '\0';
			/* Re-allocate buffer for MBS. */
			if (archive_string_ensure(as,
			    as->length + len * 2 + 1) == NULL)
				return (-1);
			p = as->s + as->length;
			end = as->s + as->buffer_length - MB_CUR_MAX -1;
		}
#if HAVE_WCRTOMB
		n = wcrtomb(p, *w++, &shift_state);
#else
		n = wctomb(p, *w++);
#endif
		if (n == -1) {
			if (errno == EILSEQ) {
				/* Skip an illegal wide char. */
				*p++ = '?';
				ret_val = -1;
			} else {
				ret_val = -1;
				break;
			}
		} else
			p += n;
		len--;
	}
	as->length = p - as->s;
	as->s[as->length] = '\0';
	return (ret_val);
}