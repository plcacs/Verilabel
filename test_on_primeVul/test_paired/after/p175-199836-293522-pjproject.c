PJ_DEF(int) pj_scan_get_char( pj_scanner *scanner )
{
    register char *s = scanner->curptr;
    int chr;

    if (s >= scanner->end || !*s) {
	pj_scan_syntax_err(scanner);
	return 0;
    }

    chr = *s;

    ++s;
    scanner->curptr = s;
    if (PJ_SCAN_CHECK_EOF(s) && PJ_SCAN_IS_PROBABLY_SPACE(*s) &&
    	scanner->skip_ws)
    {
	pj_scan_skip_whitespace(scanner);
    }
    return chr;
}