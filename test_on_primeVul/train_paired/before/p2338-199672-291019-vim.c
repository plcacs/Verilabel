ex_substitute(exarg_T *eap)
{
    linenr_T	lnum;
    long	i = 0;
    regmmatch_T regmatch;
    static subflags_T subflags = {FALSE, FALSE, FALSE, TRUE, FALSE,
							      FALSE, FALSE, 0};
#ifdef FEAT_EVAL
    subflags_T	subflags_save;
#endif
    int		save_do_all;		// remember user specified 'g' flag
    int		save_do_ask;		// remember user specified 'c' flag
    char_u	*pat = NULL, *sub = NULL;	// init for GCC
    char_u	*sub_copy = NULL;
    int		delimiter;
    int		sublen;
    int		got_quit = FALSE;
    int		got_match = FALSE;
    int		temp;
    int		which_pat;
    char_u	*cmd;
    int		save_State;
    linenr_T	first_line = 0;		// first changed line
    linenr_T	last_line= 0;		// below last changed line AFTER the
					// change
    linenr_T	old_line_count = curbuf->b_ml.ml_line_count;
    linenr_T	line2;
    long	nmatch;			// number of lines in match
    char_u	*sub_firstline;		// allocated copy of first sub line
    int		endcolumn = FALSE;	// cursor in last column when done
    pos_T	old_cursor = curwin->w_cursor;
    int		start_nsubs;
#ifdef FEAT_EVAL
    int		save_ma = 0;
#endif

    cmd = eap->arg;
    if (!global_busy)
    {
	sub_nsubs = 0;
	sub_nlines = 0;
    }
    start_nsubs = sub_nsubs;

    if (eap->cmdidx == CMD_tilde)
	which_pat = RE_LAST;	// use last used regexp
    else
	which_pat = RE_SUBST;	// use last substitute regexp

				// new pattern and substitution
    if (eap->cmd[0] == 's' && *cmd != NUL && !VIM_ISWHITE(*cmd)
		&& vim_strchr((char_u *)"0123456789cegriIp|\"", *cmd) == NULL)
    {
				// don't accept alphanumeric for separator
	if (check_regexp_delim(*cmd) == FAIL)
	    return;
#ifdef FEAT_EVAL
	if (in_vim9script() && check_global_and_subst(eap->cmd, eap->arg)
								      == FAIL)
	    return;
#endif

	/*
	 * undocumented vi feature:
	 *  "\/sub/" and "\?sub?" use last used search pattern (almost like
	 *  //sub/r).  "\&sub&" use last substitute pattern (like //sub/).
	 */
	if (*cmd == '\\')
	{
	    if (in_vim9script())
	    {
		emsg(_(e_cannot_use_s_backslash_in_vim9_script));
		return;
	    }
	    ++cmd;
	    if (vim_strchr((char_u *)"/?&", *cmd) == NULL)
	    {
		emsg(_(e_backslash_should_be_followed_by));
		return;
	    }
	    if (*cmd != '&')
		which_pat = RE_SEARCH;	    // use last '/' pattern
	    pat = (char_u *)"";		    // empty search pattern
	    delimiter = *cmd++;		    // remember delimiter character
	}
	else		// find the end of the regexp
	{
	    which_pat = RE_LAST;	    // use last used regexp
	    delimiter = *cmd++;		    // remember delimiter character
	    pat = cmd;			    // remember start of search pat
	    cmd = skip_regexp_ex(cmd, delimiter, magic_isset(),
							&eap->arg, NULL, NULL);
	    if (cmd[0] == delimiter)	    // end delimiter found
		*cmd++ = NUL;		    // replace it with a NUL
	}

	/*
	 * Small incompatibility: vi sees '\n' as end of the command, but in
	 * Vim we want to use '\n' to find/substitute a NUL.
	 */
	sub = cmd;	    // remember the start of the substitution
	cmd = skip_substitute(cmd, delimiter);

	if (!eap->skip)
	{
	    // In POSIX vi ":s/pat/%/" uses the previous subst. string.
	    if (STRCMP(sub, "%") == 0
				 && vim_strchr(p_cpo, CPO_SUBPERCENT) != NULL)
	    {
		if (old_sub == NULL)	// there is no previous command
		{
		    emsg(_(e_no_previous_substitute_regular_expression));
		    return;
		}
		sub = old_sub;
	    }
	    else
	    {
		vim_free(old_sub);
		old_sub = vim_strsave(sub);
	    }
	}
    }
    else if (!eap->skip)	// use previous pattern and substitution
    {
	if (old_sub == NULL)	// there is no previous command
	{
	    emsg(_(e_no_previous_substitute_regular_expression));
	    return;
	}
	pat = NULL;		// search_regcomp() will use previous pattern
	sub = old_sub;

	// Vi compatibility quirk: repeating with ":s" keeps the cursor in the
	// last column after using "$".
	endcolumn = (curwin->w_curswant == MAXCOL);
    }

    // Recognize ":%s/\n//" and turn it into a join command, which is much
    // more efficient.
    // TODO: find a generic solution to make line-joining operations more
    // efficient, avoid allocating a string that grows in size.
    if (pat != NULL && STRCMP(pat, "\\n") == 0
	    && *sub == NUL
	    && (*cmd == NUL || (cmd[1] == NUL && (*cmd == 'g' || *cmd == 'l'
					     || *cmd == 'p' || *cmd == '#'))))
    {
	linenr_T    joined_lines_count;

	if (eap->skip)
	    return;
	curwin->w_cursor.lnum = eap->line1;
	if (*cmd == 'l')
	    eap->flags = EXFLAG_LIST;
	else if (*cmd == '#')
	    eap->flags = EXFLAG_NR;
	else if (*cmd == 'p')
	    eap->flags = EXFLAG_PRINT;

	// The number of lines joined is the number of lines in the range plus
	// one.  One less when the last line is included.
	joined_lines_count = eap->line2 - eap->line1 + 1;
	if (eap->line2 < curbuf->b_ml.ml_line_count)
	    ++joined_lines_count;
	if (joined_lines_count > 1)
	{
	    (void)do_join(joined_lines_count, FALSE, TRUE, FALSE, TRUE);
	    sub_nsubs = joined_lines_count - 1;
	    sub_nlines = 1;
	    (void)do_sub_msg(FALSE);
	    ex_may_print(eap);
	}

	if ((cmdmod.cmod_flags & CMOD_KEEPPATTERNS) == 0)
	    save_re_pat(RE_SUBST, pat, magic_isset());
	// put pattern in history
	add_to_history(HIST_SEARCH, pat, TRUE, NUL);

	return;
    }

    /*
     * Find trailing options.  When '&' is used, keep old options.
     */
    if (*cmd == '&')
	++cmd;
    else
    {
#ifdef FEAT_EVAL
	if (in_vim9script())
	{
	    // ignore 'gdefault' and 'edcompatible'
	    subflags.do_all = FALSE;
	    subflags.do_ask = FALSE;
	}
	else
#endif
	if (!p_ed)
	{
	    if (p_gd)		// default is global on
		subflags.do_all = TRUE;
	    else
		subflags.do_all = FALSE;
	    subflags.do_ask = FALSE;
	}
	subflags.do_error = TRUE;
	subflags.do_print = FALSE;
	subflags.do_list = FALSE;
	subflags.do_count = FALSE;
	subflags.do_number = FALSE;
	subflags.do_ic = 0;
    }
    while (*cmd)
    {
	/*
	 * Note that 'g' and 'c' are always inverted, also when p_ed is off.
	 * 'r' is never inverted.
	 */
	if (*cmd == 'g')
	    subflags.do_all = !subflags.do_all;
	else if (*cmd == 'c')
	    subflags.do_ask = !subflags.do_ask;
	else if (*cmd == 'n')
	    subflags.do_count = TRUE;
	else if (*cmd == 'e')
	    subflags.do_error = !subflags.do_error;
	else if (*cmd == 'r')	    // use last used regexp
	    which_pat = RE_LAST;
	else if (*cmd == 'p')
	    subflags.do_print = TRUE;
	else if (*cmd == '#')
	{
	    subflags.do_print = TRUE;
	    subflags.do_number = TRUE;
	}
	else if (*cmd == 'l')
	{
	    subflags.do_print = TRUE;
	    subflags.do_list = TRUE;
	}
	else if (*cmd == 'i')	    // ignore case
	    subflags.do_ic = 'i';
	else if (*cmd == 'I')	    // don't ignore case
	    subflags.do_ic = 'I';
	else
	    break;
	++cmd;
    }
    if (subflags.do_count)
	subflags.do_ask = FALSE;

    save_do_all = subflags.do_all;
    save_do_ask = subflags.do_ask;

    /*
     * check for a trailing count
     */
    cmd = skipwhite(cmd);
    if (VIM_ISDIGIT(*cmd))
    {
	i = getdigits(&cmd);
	if (i <= 0 && !eap->skip && subflags.do_error)
	{
	    emsg(_(e_positive_count_required));
	    return;
	}
	eap->line1 = eap->line2;
	eap->line2 += i - 1;
	if (eap->line2 > curbuf->b_ml.ml_line_count)
	    eap->line2 = curbuf->b_ml.ml_line_count;
    }

    /*
     * check for trailing command or garbage
     */
    cmd = skipwhite(cmd);
    if (*cmd && *cmd != '"')	    // if not end-of-line or comment
    {
	set_nextcmd(eap, cmd);
	if (eap->nextcmd == NULL)
	{
	    semsg(_(e_trailing_characters_str), cmd);
	    return;
	}
    }

    if (eap->skip)	    // not executing commands, only parsing
	return;

    if (!subflags.do_count && !curbuf->b_p_ma)
    {
	// Substitution is not allowed in non-'modifiable' buffer
	emsg(_(e_cannot_make_changes_modifiable_is_off));
	return;
    }

    if (search_regcomp(pat, RE_SUBST, which_pat, SEARCH_HIS, &regmatch) == FAIL)
    {
	if (subflags.do_error)
	    emsg(_(e_invalid_command));
	return;
    }

    // the 'i' or 'I' flag overrules 'ignorecase' and 'smartcase'
    if (subflags.do_ic == 'i')
	regmatch.rmm_ic = TRUE;
    else if (subflags.do_ic == 'I')
	regmatch.rmm_ic = FALSE;

    sub_firstline = NULL;

    /*
     * If the substitute pattern starts with "\=" then it's an expression.
     * Make a copy, a recursive function may free it.
     * Otherwise, '~' in the substitute pattern is replaced with the old
     * pattern.  We do it here once to avoid it to be replaced over and over
     * again.
     */
    if (sub[0] == '\\' && sub[1] == '=')
    {
	sub = vim_strsave(sub);
	if (sub == NULL)
	    return;
	sub_copy = sub;
    }
    else
	sub = regtilde(sub, magic_isset());

    /*
     * Check for a match on each line.
     */
    line2 = eap->line2;
    for (lnum = eap->line1; lnum <= line2 && !(got_quit
#if defined(FEAT_EVAL)
		|| aborting()
#endif
		); ++lnum)
    {
	nmatch = vim_regexec_multi(&regmatch, curwin, curbuf, lnum,
						       (colnr_T)0, NULL, NULL);
	if (nmatch)
	{
	    colnr_T	copycol;
	    colnr_T	matchcol;
	    colnr_T	prev_matchcol = MAXCOL;
	    char_u	*new_end, *new_start = NULL;
	    unsigned	new_start_len = 0;
	    char_u	*p1;
	    int		did_sub = FALSE;
	    int		lastone;
	    int		len, copy_len, needed_len;
	    long	nmatch_tl = 0;	// nr of lines matched below lnum
	    int		do_again;	// do it again after joining lines
	    int		skip_match = FALSE;
	    linenr_T	sub_firstlnum;	// nr of first sub line
#ifdef FEAT_PROP_POPUP
	    int		apc_flags = APC_SAVE_FOR_UNDO | APC_SUBSTITUTE;
	    colnr_T	total_added =  0;
#endif

	    /*
	     * The new text is build up step by step, to avoid too much
	     * copying.  There are these pieces:
	     * sub_firstline	The old text, unmodified.
	     * copycol		Column in the old text where we started
	     *			looking for a match; from here old text still
	     *			needs to be copied to the new text.
	     * matchcol		Column number of the old text where to look
	     *			for the next match.  It's just after the
	     *			previous match or one further.
	     * prev_matchcol	Column just after the previous match (if any).
	     *			Mostly equal to matchcol, except for the first
	     *			match and after skipping an empty match.
	     * regmatch.*pos	Where the pattern matched in the old text.
	     * new_start	The new text, all that has been produced so
	     *			far.
	     * new_end		The new text, where to append new text.
	     *
	     * lnum		The line number where we found the start of
	     *			the match.  Can be below the line we searched
	     *			when there is a \n before a \zs in the
	     *			pattern.
	     * sub_firstlnum	The line number in the buffer where to look
	     *			for a match.  Can be different from "lnum"
	     *			when the pattern or substitute string contains
	     *			line breaks.
	     *
	     * Special situations:
	     * - When the substitute string contains a line break, the part up
	     *   to the line break is inserted in the text, but the copy of
	     *   the original line is kept.  "sub_firstlnum" is adjusted for
	     *   the inserted lines.
	     * - When the matched pattern contains a line break, the old line
	     *   is taken from the line at the end of the pattern.  The lines
	     *   in the match are deleted later, "sub_firstlnum" is adjusted
	     *   accordingly.
	     *
	     * The new text is built up in new_start[].  It has some extra
	     * room to avoid using alloc()/free() too often.  new_start_len is
	     * the length of the allocated memory at new_start.
	     *
	     * Make a copy of the old line, so it won't be taken away when
	     * updating the screen or handling a multi-line match.  The "old_"
	     * pointers point into this copy.
	     */
	    sub_firstlnum = lnum;
	    copycol = 0;
	    matchcol = 0;

	    // At first match, remember current cursor position.
	    if (!got_match)
	    {
		setpcmark();
		got_match = TRUE;
	    }

	    /*
	     * Loop until nothing more to replace in this line.
	     * 1. Handle match with empty string.
	     * 2. If do_ask is set, ask for confirmation.
	     * 3. substitute the string.
	     * 4. if do_all is set, find next match
	     * 5. break if there isn't another match in this line
	     */
	    for (;;)
	    {
		// Advance "lnum" to the line where the match starts.  The
		// match does not start in the first line when there is a line
		// break before \zs.
		if (regmatch.startpos[0].lnum > 0)
		{
		    lnum += regmatch.startpos[0].lnum;
		    sub_firstlnum += regmatch.startpos[0].lnum;
		    nmatch -= regmatch.startpos[0].lnum;
		    VIM_CLEAR(sub_firstline);
		}

		// Match might be after the last line for "\n\zs" matching at
		// the end of the last line.
		if (lnum > curbuf->b_ml.ml_line_count)
		    break;

		if (sub_firstline == NULL)
		{
		    sub_firstline = vim_strsave(ml_get(sub_firstlnum));
		    if (sub_firstline == NULL)
		    {
			vim_free(new_start);
			goto outofmem;
		    }
		}

		// Save the line number of the last change for the final
		// cursor position (just like Vi).
		curwin->w_cursor.lnum = lnum;
		do_again = FALSE;

		/*
		 * 1. Match empty string does not count, except for first
		 * match.  This reproduces the strange vi behaviour.
		 * This also catches endless loops.
		 */
		if (matchcol == prev_matchcol
			&& regmatch.endpos[0].lnum == 0
			&& matchcol == regmatch.endpos[0].col)
		{
		    if (sub_firstline[matchcol] == NUL)
			// We already were at the end of the line.  Don't look
			// for a match in this line again.
			skip_match = TRUE;
		    else
		    {
			 // search for a match at next column
			if (has_mbyte)
			    matchcol += mb_ptr2len(sub_firstline + matchcol);
			else
			    ++matchcol;
		    }
		    goto skip;
		}

		// Normally we continue searching for a match just after the
		// previous match.
		matchcol = regmatch.endpos[0].col;
		prev_matchcol = matchcol;

		/*
		 * 2. If do_count is set only increase the counter.
		 *    If do_ask is set, ask for confirmation.
		 */
		if (subflags.do_count)
		{
		    // For a multi-line match, put matchcol at the NUL at
		    // the end of the line and set nmatch to one, so that
		    // we continue looking for a match on the next line.
		    // Avoids that ":s/\nB\@=//gc" get stuck.
		    if (nmatch > 1)
		    {
			matchcol = (colnr_T)STRLEN(sub_firstline);
			nmatch = 1;
			skip_match = TRUE;
		    }
		    sub_nsubs++;
		    did_sub = TRUE;
#ifdef FEAT_EVAL
		    // Skip the substitution, unless an expression is used,
		    // then it is evaluated in the sandbox.
		    if (!(sub[0] == '\\' && sub[1] == '='))
#endif
			goto skip;
		}

		if (subflags.do_ask)
		{
		    int typed = 0;

		    // change State to MODE_CONFIRM, so that the mouse works
		    // properly
		    save_State = State;
		    State = MODE_CONFIRM;
		    setmouse();		// disable mouse in xterm
		    curwin->w_cursor.col = regmatch.startpos[0].col;
		    if (curwin->w_p_crb)
			do_check_cursorbind();

		    // When 'cpoptions' contains "u" don't sync undo when
		    // asking for confirmation.
		    if (vim_strchr(p_cpo, CPO_UNDO) != NULL)
			++no_u_sync;

		    /*
		     * Loop until 'y', 'n', 'q', CTRL-E or CTRL-Y typed.
		     */
		    while (subflags.do_ask)
		    {
			if (exmode_active)
			{
			    char_u	*resp;
			    colnr_T	sc, ec;

			    print_line_no_prefix(lnum,
					 subflags.do_number, subflags.do_list);

			    getvcol(curwin, &curwin->w_cursor, &sc, NULL, NULL);
			    curwin->w_cursor.col = regmatch.endpos[0].col - 1;
			    if (curwin->w_cursor.col < 0)
				curwin->w_cursor.col = 0;
			    getvcol(curwin, &curwin->w_cursor, NULL, NULL, &ec);
			    curwin->w_cursor.col = regmatch.startpos[0].col;
			    if (subflags.do_number || curwin->w_p_nu)
			    {
				int numw = number_width(curwin) + 1;
				sc += numw;
				ec += numw;
			    }
			    msg_start();
			    for (i = 0; i < (long)sc; ++i)
				msg_putchar(' ');
			    for ( ; i <= (long)ec; ++i)
				msg_putchar('^');

			    resp = getexmodeline('?', NULL, 0, TRUE);
			    if (resp != NULL)
			    {
				typed = *resp;
				vim_free(resp);
				// When ":normal" runs out of characters we get
				// an empty line.  Use "q" to get out of the
				// loop.
				if (ex_normal_busy && typed == NUL)
				    typed = 'q';
			    }
			}
			else
			{
			    char_u *orig_line = NULL;
			    int    len_change = 0;
			    int	   save_p_lz = p_lz;
#ifdef FEAT_FOLDING
			    int save_p_fen = curwin->w_p_fen;

			    curwin->w_p_fen = FALSE;
#endif
			    // Invert the matched string.
			    // Remove the inversion afterwards.
			    temp = RedrawingDisabled;
			    RedrawingDisabled = 0;

			    // avoid calling update_screen() in vgetorpeek()
			    p_lz = FALSE;

			    if (new_start != NULL)
			    {
				// There already was a substitution, we would
				// like to show this to the user.  We cannot
				// really update the line, it would change
				// what matches.  Temporarily replace the line
				// and change it back afterwards.
				orig_line = vim_strsave(ml_get(lnum));
				if (orig_line != NULL)
				{
				    char_u *new_line = concat_str(new_start,
						     sub_firstline + copycol);

				    if (new_line == NULL)
					VIM_CLEAR(orig_line);
				    else
				    {
					// Position the cursor relative to the
					// end of the line, the previous
					// substitute may have inserted or
					// deleted characters before the
					// cursor.
					len_change = (int)STRLEN(new_line)
						     - (int)STRLEN(orig_line);
					curwin->w_cursor.col += len_change;
					ml_replace(lnum, new_line, FALSE);
				    }
				}
			    }

			    search_match_lines = regmatch.endpos[0].lnum
						  - regmatch.startpos[0].lnum;
			    search_match_endcol = regmatch.endpos[0].col
								 + len_change;
			    highlight_match = TRUE;

			    update_topline();
			    validate_cursor();
			    update_screen(SOME_VALID);
			    highlight_match = FALSE;
			    redraw_later(SOME_VALID);

#ifdef FEAT_FOLDING
			    curwin->w_p_fen = save_p_fen;
#endif
			    if (msg_row == Rows - 1)
				msg_didout = FALSE;	// avoid a scroll-up
			    msg_starthere();
			    i = msg_scroll;
			    msg_scroll = 0;		// truncate msg when
							// needed
			    msg_no_more = TRUE;
			    // write message same highlighting as for
			    // wait_return
			    smsg_attr(HL_ATTR(HLF_R),
				_("replace with %s (y/n/a/q/l/^E/^Y)?"), sub);
			    msg_no_more = FALSE;
			    msg_scroll = i;
			    showruler(TRUE);
			    windgoto(msg_row, msg_col);
			    RedrawingDisabled = temp;

#ifdef USE_ON_FLY_SCROLL
			    dont_scroll = FALSE; // allow scrolling here
#endif
			    ++no_mapping;	// don't map this key
			    ++allow_keys;	// allow special keys
			    typed = plain_vgetc();
			    --allow_keys;
			    --no_mapping;

			    // clear the question
			    msg_didout = FALSE;	// don't scroll up
			    msg_col = 0;
			    gotocmdline(TRUE);
			    p_lz = save_p_lz;

			    // restore the line
			    if (orig_line != NULL)
				ml_replace(lnum, orig_line, FALSE);
			}

			need_wait_return = FALSE; // no hit-return prompt
			if (typed == 'q' || typed == ESC || typed == Ctrl_C
#ifdef UNIX
				|| typed == intr_char
#endif
				)
			{
			    got_quit = TRUE;
			    break;
			}
			if (typed == 'n')
			    break;
			if (typed == 'y')
			    break;
			if (typed == 'l')
			{
			    // last: replace and then stop
			    subflags.do_all = FALSE;
			    line2 = lnum;
			    break;
			}
			if (typed == 'a')
			{
			    subflags.do_ask = FALSE;
			    break;
			}
			if (typed == Ctrl_E)
			    scrollup_clamp();
			else if (typed == Ctrl_Y)
			    scrolldown_clamp();
		    }
		    State = save_State;
		    setmouse();
		    if (vim_strchr(p_cpo, CPO_UNDO) != NULL)
			--no_u_sync;

		    if (typed == 'n')
		    {
			// For a multi-line match, put matchcol at the NUL at
			// the end of the line and set nmatch to one, so that
			// we continue looking for a match on the next line.
			// Avoids that ":%s/\nB\@=//gc" and ":%s/\n/,\r/gc"
			// get stuck when pressing 'n'.
			if (nmatch > 1)
			{
			    matchcol = (colnr_T)STRLEN(sub_firstline);
			    skip_match = TRUE;
			}
			goto skip;
		    }
		    if (got_quit)
			goto skip;
		}

		// Move the cursor to the start of the match, so that we can
		// use "\=col(".").
		curwin->w_cursor.col = regmatch.startpos[0].col;

		/*
		 * 3. substitute the string.
		 */
#ifdef FEAT_EVAL
		save_ma = curbuf->b_p_ma;
		if (subflags.do_count)
		{
		    // prevent accidentally changing the buffer by a function
		    curbuf->b_p_ma = FALSE;
		    sandbox++;
		}
		// Save flags for recursion.  They can change for e.g.
		// :s/^/\=execute("s#^##gn")
		subflags_save = subflags;
#endif
		// get length of substitution part
		sublen = vim_regsub_multi(&regmatch,
				    sub_firstlnum - regmatch.startpos[0].lnum,
			       sub, sub_firstline, FALSE, magic_isset(), TRUE);
#ifdef FEAT_EVAL
		// If getting the substitute string caused an error, don't do
		// the replacement.
		// Don't keep flags set by a recursive call.
		subflags = subflags_save;
		if (aborting() || subflags.do_count)
		{
		    curbuf->b_p_ma = save_ma;
		    if (sandbox > 0)
			sandbox--;
		    goto skip;
		}
#endif

		// When the match included the "$" of the last line it may
		// go beyond the last line of the buffer.
		if (nmatch > curbuf->b_ml.ml_line_count - sub_firstlnum + 1)
		{
		    nmatch = curbuf->b_ml.ml_line_count - sub_firstlnum + 1;
		    skip_match = TRUE;
		}

		// Need room for:
		// - result so far in new_start (not for first sub in line)
		// - original text up to match
		// - length of substituted part
		// - original text after match
		// Adjust text properties here, since we have all information
		// needed.
		if (nmatch == 1)
		{
		    p1 = sub_firstline;
#ifdef FEAT_PROP_POPUP
		    if (curbuf->b_has_textprop)
		    {
			int bytes_added = sublen - 1 - (regmatch.endpos[0].col
						   - regmatch.startpos[0].col);

			// When text properties are changed, need to save for
			// undo first, unless done already.
			if (adjust_prop_columns(lnum,
					total_added + regmatch.startpos[0].col,
						       bytes_added, apc_flags))
			    apc_flags &= ~APC_SAVE_FOR_UNDO;
			// Offset for column byte number of the text property
			// in the resulting buffer afterwards.
			total_added += bytes_added;
		    }
#endif
		}
		else
		{
		    p1 = ml_get(sub_firstlnum + nmatch - 1);
		    nmatch_tl += nmatch - 1;
		}
		copy_len = regmatch.startpos[0].col - copycol;
		needed_len = copy_len + ((unsigned)STRLEN(p1)
				       - regmatch.endpos[0].col) + sublen + 1;
		if (new_start == NULL)
		{
		    /*
		     * Get some space for a temporary buffer to do the
		     * substitution into (and some extra space to avoid
		     * too many calls to alloc()/free()).
		     */
		    new_start_len = needed_len + 50;
		    if ((new_start = alloc(new_start_len)) == NULL)
			goto outofmem;
		    *new_start = NUL;
		    new_end = new_start;
		}
		else
		{
		    /*
		     * Check if the temporary buffer is long enough to do the
		     * substitution into.  If not, make it larger (with a bit
		     * extra to avoid too many calls to alloc()/free()).
		     */
		    len = (unsigned)STRLEN(new_start);
		    needed_len += len;
		    if (needed_len > (int)new_start_len)
		    {
			new_start_len = needed_len + 50;
			if ((p1 = alloc(new_start_len)) == NULL)
			{
			    vim_free(new_start);
			    goto outofmem;
			}
			mch_memmove(p1, new_start, (size_t)(len + 1));
			vim_free(new_start);
			new_start = p1;
		    }
		    new_end = new_start + len;
		}

		/*
		 * copy the text up to the part that matched
		 */
		mch_memmove(new_end, sub_firstline + copycol, (size_t)copy_len);
		new_end += copy_len;

		(void)vim_regsub_multi(&regmatch,
				    sub_firstlnum - regmatch.startpos[0].lnum,
				      sub, new_end, TRUE, magic_isset(), TRUE);
		sub_nsubs++;
		did_sub = TRUE;

		// Move the cursor to the start of the line, to avoid that it
		// is beyond the end of the line after the substitution.
		curwin->w_cursor.col = 0;

		// For a multi-line match, make a copy of the last matched
		// line and continue in that one.
		if (nmatch > 1)
		{
		    sub_firstlnum += nmatch - 1;
		    vim_free(sub_firstline);
		    sub_firstline = vim_strsave(ml_get(sub_firstlnum));
		    // When going beyond the last line, stop substituting.
		    if (sub_firstlnum <= line2)
			do_again = TRUE;
		    else
			subflags.do_all = FALSE;
		}

		// Remember next character to be copied.
		copycol = regmatch.endpos[0].col;

		if (skip_match)
		{
		    // Already hit end of the buffer, sub_firstlnum is one
		    // less than what it ought to be.
		    vim_free(sub_firstline);
		    sub_firstline = vim_strsave((char_u *)"");
		    copycol = 0;
		}

		/*
		 * Now the trick is to replace CTRL-M chars with a real line
		 * break.  This would make it impossible to insert a CTRL-M in
		 * the text.  The line break can be avoided by preceding the
		 * CTRL-M with a backslash.  To be able to insert a backslash,
		 * they must be doubled in the string and are halved here.
		 * That is Vi compatible.
		 */
		for (p1 = new_end; *p1; ++p1)
		{
		    if (p1[0] == '\\' && p1[1] != NUL)  // remove backslash
		    {
			STRMOVE(p1, p1 + 1);
#ifdef FEAT_PROP_POPUP
			if (curbuf->b_has_textprop)
			{
			    // When text properties are changed, need to save
			    // for undo first, unless done already.
			    if (adjust_prop_columns(lnum,
					(colnr_T)(p1 - new_start), -1,
					apc_flags))
				apc_flags &= ~APC_SAVE_FOR_UNDO;
			}
#endif
		    }
		    else if (*p1 == CAR)
		    {
			if (u_inssub(lnum) == OK)   // prepare for undo
			{
			    colnr_T	plen = (colnr_T)(p1 - new_start + 1);

			    *p1 = NUL;		    // truncate up to the CR
			    ml_append(lnum - 1, new_start, plen, FALSE);
			    mark_adjust(lnum + 1, (linenr_T)MAXLNUM, 1L, 0L);
			    if (subflags.do_ask)
				appended_lines(lnum - 1, 1L);
			    else
			    {
				if (first_line == 0)
				    first_line = lnum;
				last_line = lnum + 1;
			    }
#ifdef FEAT_PROP_POPUP
			    adjust_props_for_split(lnum + 1, lnum, plen, 1);
#endif
			    // all line numbers increase
			    ++sub_firstlnum;
			    ++lnum;
			    ++line2;
			    // move the cursor to the new line, like Vi
			    ++curwin->w_cursor.lnum;
			    // copy the rest
			    STRMOVE(new_start, p1 + 1);
			    p1 = new_start - 1;
			}
		    }
		    else if (has_mbyte)
			p1 += (*mb_ptr2len)(p1) - 1;
		}

		/*
		 * 4. If do_all is set, find next match.
		 * Prevent endless loop with patterns that match empty
		 * strings, e.g. :s/$/pat/g or :s/[a-z]* /(&)/g.
		 * But ":s/\n/#/" is OK.
		 */
skip:
		// We already know that we did the last subst when we are at
		// the end of the line, except that a pattern like
		// "bar\|\nfoo" may match at the NUL.  "lnum" can be below
		// "line2" when there is a \zs in the pattern after a line
		// break.
		lastone = (skip_match
			|| got_int
			|| got_quit
			|| lnum > line2
			|| !(subflags.do_all || do_again)
			|| (sub_firstline[matchcol] == NUL && nmatch <= 1
					 && !re_multiline(regmatch.regprog)));
		nmatch = -1;

		/*
		 * Replace the line in the buffer when needed.  This is
		 * skipped when there are more matches.
		 * The check for nmatch_tl is needed for when multi-line
		 * matching must replace the lines before trying to do another
		 * match, otherwise "\@<=" won't work.
		 * When the match starts below where we start searching also
		 * need to replace the line first (using \zs after \n).
		 */
		if (lastone
			|| nmatch_tl > 0
			|| (nmatch = vim_regexec_multi(&regmatch, curwin,
							curbuf, sub_firstlnum,
						    matchcol, NULL, NULL)) == 0
			|| regmatch.startpos[0].lnum > 0)
		{
		    if (new_start != NULL)
		    {
			/*
			 * Copy the rest of the line, that didn't match.
			 * "matchcol" has to be adjusted, we use the end of
			 * the line as reference, because the substitute may
			 * have changed the number of characters.  Same for
			 * "prev_matchcol".
			 */
			STRCAT(new_start, sub_firstline + copycol);
			matchcol = (colnr_T)STRLEN(sub_firstline) - matchcol;
			prev_matchcol = (colnr_T)STRLEN(sub_firstline)
							      - prev_matchcol;

			if (u_savesub(lnum) != OK)
			    break;
			ml_replace(lnum, new_start, TRUE);

			if (nmatch_tl > 0)
			{
			    /*
			     * Matched lines have now been substituted and are
			     * useless, delete them.  The part after the match
			     * has been appended to new_start, we don't need
			     * it in the buffer.
			     */
			    ++lnum;
			    if (u_savedel(lnum, nmatch_tl) != OK)
				break;
			    for (i = 0; i < nmatch_tl; ++i)
				ml_delete(lnum);
			    mark_adjust(lnum, lnum + nmatch_tl - 1,
						   (long)MAXLNUM, -nmatch_tl);
			    if (subflags.do_ask)
				deleted_lines(lnum, nmatch_tl);
			    --lnum;
			    line2 -= nmatch_tl; // nr of lines decreases
			    nmatch_tl = 0;
			}

			// When asking, undo is saved each time, must also set
			// changed flag each time.
			if (subflags.do_ask)
			    changed_bytes(lnum, 0);
			else
			{
			    if (first_line == 0)
				first_line = lnum;
			    last_line = lnum + 1;
			}

			sub_firstlnum = lnum;
			vim_free(sub_firstline);    // free the temp buffer
			sub_firstline = new_start;
			new_start = NULL;
			matchcol = (colnr_T)STRLEN(sub_firstline) - matchcol;
			prev_matchcol = (colnr_T)STRLEN(sub_firstline)
							      - prev_matchcol;
			copycol = 0;
		    }
		    if (nmatch == -1 && !lastone)
			nmatch = vim_regexec_multi(&regmatch, curwin, curbuf,
					  sub_firstlnum, matchcol, NULL, NULL);

		    /*
		     * 5. break if there isn't another match in this line
		     */
		    if (nmatch <= 0)
		    {
			// If the match found didn't start where we were
			// searching, do the next search in the line where we
			// found the match.
			if (nmatch == -1)
			    lnum -= regmatch.startpos[0].lnum;
			break;
		    }
		}

		line_breakcheck();
	    }

	    if (did_sub)
		++sub_nlines;
	    vim_free(new_start);	// for when substitute was cancelled
	    VIM_CLEAR(sub_firstline);	// free the copy of the original line
	}

	line_breakcheck();
    }

    if (first_line != 0)
    {
	// Need to subtract the number of added lines from "last_line" to get
	// the line number before the change (same as adding the number of
	// deleted lines).
	i = curbuf->b_ml.ml_line_count - old_line_count;
	changed_lines(first_line, 0, last_line - i, i);
    }

outofmem:
    vim_free(sub_firstline); // may have to free allocated copy of the line

    // ":s/pat//n" doesn't move the cursor
    if (subflags.do_count)
	curwin->w_cursor = old_cursor;

    if (sub_nsubs > start_nsubs)
    {
	if ((cmdmod.cmod_flags & CMOD_LOCKMARKS) == 0)
	{
	    // Set the '[ and '] marks.
	    curbuf->b_op_start.lnum = eap->line1;
	    curbuf->b_op_end.lnum = line2;
	    curbuf->b_op_start.col = curbuf->b_op_end.col = 0;
	}

	if (!global_busy)
	{
	    // when interactive leave cursor on the match
	    if (!subflags.do_ask)
	    {
		if (endcolumn)
		    coladvance((colnr_T)MAXCOL);
		else
		    beginline(BL_WHITE | BL_FIX);
	    }
	    if (!do_sub_msg(subflags.do_count) && subflags.do_ask)
		msg("");
	}
	else
	    global_need_beginline = TRUE;
	if (subflags.do_print)
	    print_line(curwin->w_cursor.lnum,
					 subflags.do_number, subflags.do_list);
    }
    else if (!global_busy)
    {
	if (got_int)		// interrupted
	    emsg(_(e_interrupted));
	else if (got_match)	// did find something but nothing substituted
	    msg("");
	else if (subflags.do_error)	// nothing found
	    semsg(_(e_pattern_not_found_str), get_search_pat());
    }

#ifdef FEAT_FOLDING
    if (subflags.do_ask && hasAnyFolding(curwin))
	// Cursor position may require updating
	changed_window_setting();
#endif

    vim_regfree(regmatch.regprog);
    vim_free(sub_copy);

    // Restore the flag values, they can be used for ":&&".
    subflags.do_all = save_do_all;
    subflags.do_ask = save_do_ask;
}