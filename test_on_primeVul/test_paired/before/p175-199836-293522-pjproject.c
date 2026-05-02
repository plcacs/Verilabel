PJ_DEF(int) pj_scan_get_char( pj_scanner *scanner )
{
    int chr = *scanner->curptr;

    if (!chr) {
	pj_scan_syntax_err(scanner);
	return 0;
    }

    ++scanner->curptr;

    if (PJ_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && scanner->skip_ws) {
	pj_scan_skip_whitespace(scanner);
    }
    return chr;
}