ex_open(exarg_T *eap)
{
    regmatch_T	regmatch;
    char_u	*p;

#ifdef FEAT_EVAL
    if (not_in_vim9(eap) == FAIL)
	return;
#endif
    curwin->w_cursor.lnum = eap->line2;
    beginline(BL_SOL | BL_FIX);
    if (*eap->arg == '/')
    {
	// ":open /pattern/": put cursor in column found with pattern
	++eap->arg;
	p = skip_regexp(eap->arg, '/', magic_isset());
	*p = NUL;
	regmatch.regprog = vim_regcomp(eap->arg, magic_isset() ? RE_MAGIC : 0);
	if (regmatch.regprog != NULL)
	{
	    // make a copy of the line, when searching for a mark it might be
	    // flushed
	    char_u *line = vim_strsave(ml_get_curline());

	    regmatch.rm_ic = p_ic;
	    if (vim_regexec(&regmatch, line, (colnr_T)0))
		curwin->w_cursor.col = (colnr_T)(regmatch.startp[0] - line);
	    else
		emsg(_(e_nomatch));
	    vim_regfree(regmatch.regprog);
	    vim_free(line);
	}
	// Move to the NUL, ignore any other arguments.
	eap->arg += STRLEN(eap->arg);
    }
    check_cursor();

    eap->cmdidx = CMD_visual;
    do_exedit(eap, NULL);
}