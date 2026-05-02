find_match_text(colnr_T startcol, int regstart, char_u *match_text)
{
    colnr_T col = startcol;
    int	    c1, c2;
    int	    len1, len2;
    int	    match;

    for (;;)
    {
	match = TRUE;
	len2 = MB_CHAR2LEN(regstart); // skip regstart
	for (len1 = 0; match_text[len1] != NUL; len1 += MB_CHAR2LEN(c1))
	{
	    c1 = PTR2CHAR(match_text + len1);
	    c2 = PTR2CHAR(rex.line + col + len2);
	    if (c1 != c2 && (!rex.reg_ic || MB_CASEFOLD(c1) != MB_CASEFOLD(c2)))
	    {
		match = FALSE;
		break;
	    }
	    len2 += MB_CHAR2LEN(c2);
	}
	if (match
		// check that no composing char follows
		&& !(enc_utf8
			  && utf_iscomposing(PTR2CHAR(rex.line + col + len2))))
	{
	    cleanup_subexpr();
	    if (REG_MULTI)
	    {
		rex.reg_startpos[0].lnum = rex.lnum;
		rex.reg_startpos[0].col = col;
		rex.reg_endpos[0].lnum = rex.lnum;
		rex.reg_endpos[0].col = col + len2;
	    }
	    else
	    {
		rex.reg_startp[0] = rex.line + col;
		rex.reg_endp[0] = rex.line + col + len2;
	    }
	    return 1L;
	}

	// Try finding regstart after the current match.
	col += MB_CHAR2LEN(regstart); // skip regstart
	if (skip_to_start(regstart, &col) == FAIL)
	    break;
    }
    return 0L;
}