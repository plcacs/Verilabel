ins_bs(
    int		c,
    int		mode,
    int		*inserted_space_p)
{
    linenr_T	lnum;
    int		cc;
    int		temp = 0;	    // init for GCC
    colnr_T	save_col;
    colnr_T	mincol;
    int		did_backspace = FALSE;
    int		in_indent;
    int		oldState;
    int		cpc[MAX_MCO];	    // composing characters
    int		call_fix_indent = FALSE;

    /*
     * can't delete anything in an empty file
     * can't backup past first character in buffer
     * can't backup past starting point unless 'backspace' > 1
     * can backup to a previous line if 'backspace' == 0
     */
    if (       BUFEMPTY()
	    || (
#ifdef FEAT_RIGHTLEFT
		!revins_on &&
#endif
		((curwin->w_cursor.lnum == 1 && curwin->w_cursor.col == 0)
		    || (!can_bs(BS_START)
			&& ((arrow_used
#ifdef FEAT_JOB_CHANNEL
				&& !bt_prompt(curbuf)
#endif
			) || (curwin->w_cursor.lnum == Insstart_orig.lnum
				&& curwin->w_cursor.col <= Insstart_orig.col)))
		    || (!can_bs(BS_INDENT) && !arrow_used && ai_col > 0
					 && curwin->w_cursor.col <= ai_col)
		    || (!can_bs(BS_EOL) && curwin->w_cursor.col == 0))))
    {
	vim_beep(BO_BS);
	return FALSE;
    }

    if (stop_arrow() == FAIL)
	return FALSE;
    in_indent = inindent(0);
    if (in_indent)
	can_cindent = FALSE;
    end_comment_pending = NUL;	// After BS, don't auto-end comment
#ifdef FEAT_RIGHTLEFT
    if (revins_on)	    // put cursor after last inserted char
	inc_cursor();
#endif

    // Virtualedit:
    //	BACKSPACE_CHAR eats a virtual space
    //	BACKSPACE_WORD eats all coladd
    //	BACKSPACE_LINE eats all coladd and keeps going
    if (curwin->w_cursor.coladd > 0)
    {
	if (mode == BACKSPACE_CHAR)
	{
	    --curwin->w_cursor.coladd;
	    return TRUE;
	}
	if (mode == BACKSPACE_WORD)
	{
	    curwin->w_cursor.coladd = 0;
	    return TRUE;
	}
	curwin->w_cursor.coladd = 0;
    }

    /*
     * Delete newline!
     */
    if (curwin->w_cursor.col == 0)
    {
	lnum = Insstart.lnum;
	if (curwin->w_cursor.lnum == lnum
#ifdef FEAT_RIGHTLEFT
			|| revins_on
#endif
				    )
	{
	    if (u_save((linenr_T)(curwin->w_cursor.lnum - 2),
			       (linenr_T)(curwin->w_cursor.lnum + 1)) == FAIL)
		return FALSE;
	    --Insstart.lnum;
	    Insstart.col = (colnr_T)STRLEN(ml_get(Insstart.lnum));
	}
	/*
	 * In replace mode:
	 * cc < 0: NL was inserted, delete it
	 * cc >= 0: NL was replaced, put original characters back
	 */
	cc = -1;
	if (State & REPLACE_FLAG)
	    cc = replace_pop();	    // returns -1 if NL was inserted
	/*
	 * In replace mode, in the line we started replacing, we only move the
	 * cursor.
	 */
	if ((State & REPLACE_FLAG) && curwin->w_cursor.lnum <= lnum)
	{
	    dec_cursor();
	}
	else
	{
	    if (!(State & VREPLACE_FLAG)
				   || curwin->w_cursor.lnum > orig_line_count)
	    {
		temp = gchar_cursor();	// remember current char
		--curwin->w_cursor.lnum;

		// When "aw" is in 'formatoptions' we must delete the space at
		// the end of the line, otherwise the line will be broken
		// again when auto-formatting.
		if (has_format_option(FO_AUTO)
					   && has_format_option(FO_WHITE_PAR))
		{
		    char_u  *ptr = ml_get_buf(curbuf, curwin->w_cursor.lnum,
									TRUE);
		    int	    len;

		    len = (int)STRLEN(ptr);
		    if (len > 0 && ptr[len - 1] == ' ')
			ptr[len - 1] = NUL;
		}

		(void)do_join(2, FALSE, FALSE, FALSE, FALSE);
		if (temp == NUL && gchar_cursor() != NUL)
		    inc_cursor();
	    }
	    else
		dec_cursor();

	    /*
	     * In MODE_REPLACE mode we have to put back the text that was
	     * replaced by the NL. On the replace stack is first a
	     * NUL-terminated sequence of characters that were deleted and then
	     * the characters that NL replaced.
	     */
	    if (State & REPLACE_FLAG)
	    {
		/*
		 * Do the next ins_char() in MODE_NORMAL state, to
		 * prevent ins_char() from replacing characters and
		 * avoiding showmatch().
		 */
		oldState = State;
		State = MODE_NORMAL;
		/*
		 * restore characters (blanks) deleted after cursor
		 */
		while (cc > 0)
		{
		    save_col = curwin->w_cursor.col;
		    mb_replace_pop_ins(cc);
		    curwin->w_cursor.col = save_col;
		    cc = replace_pop();
		}
		// restore the characters that NL replaced
		replace_pop_ins();
		State = oldState;
	    }
	}
	did_ai = FALSE;
    }
    else
    {
	/*
	 * Delete character(s) before the cursor.
	 */
#ifdef FEAT_RIGHTLEFT
	if (revins_on)		// put cursor on last inserted char
	    dec_cursor();
#endif
	mincol = 0;
						// keep indent
	if (mode == BACKSPACE_LINE
		&& (curbuf->b_p_ai || cindent_on())
#ifdef FEAT_RIGHTLEFT
		&& !revins_on
#endif
			    )
	{
	    save_col = curwin->w_cursor.col;
	    beginline(BL_WHITE);
	    if (curwin->w_cursor.col < save_col)
	    {
		mincol = curwin->w_cursor.col;
		// should now fix the indent to match with the previous line
		call_fix_indent = TRUE;
	    }
	    curwin->w_cursor.col = save_col;
	}

	/*
	 * Handle deleting one 'shiftwidth' or 'softtabstop'.
	 */
	if (	   mode == BACKSPACE_CHAR
		&& ((p_sta && in_indent)
		    || ((get_sts_value() != 0
#ifdef FEAT_VARTABS
			|| tabstop_count(curbuf->b_p_vsts_array)
#endif
			)
			&& curwin->w_cursor.col > 0
			&& (*(ml_get_cursor() - 1) == TAB
			    || (*(ml_get_cursor() - 1) == ' '
				&& (!*inserted_space_p
				    || arrow_used))))))
	{
	    int		ts;
	    colnr_T	vcol;
	    colnr_T	want_vcol;
	    colnr_T	start_vcol;

	    *inserted_space_p = FALSE;
	    // Compute the virtual column where we want to be.  Since
	    // 'showbreak' may get in the way, need to get the last column of
	    // the previous character.
	    getvcol(curwin, &curwin->w_cursor, &vcol, NULL, NULL);
	    start_vcol = vcol;
	    dec_cursor();
	    getvcol(curwin, &curwin->w_cursor, NULL, NULL, &want_vcol);
	    inc_cursor();
#ifdef FEAT_VARTABS
	    if (p_sta && in_indent)
	    {
		ts = (int)get_sw_value(curbuf);
		want_vcol = (want_vcol / ts) * ts;
	    }
	    else
		want_vcol = tabstop_start(want_vcol, get_sts_value(),
						       curbuf->b_p_vsts_array);
#else
	    if (p_sta && in_indent)
		ts = (int)get_sw_value(curbuf);
	    else
		ts = (int)get_sts_value();
	    want_vcol = (want_vcol / ts) * ts;
#endif

	    // delete characters until we are at or before want_vcol
	    while (vcol > want_vcol
		    && (cc = *(ml_get_cursor() - 1), VIM_ISWHITE(cc)))
		ins_bs_one(&vcol);

	    // insert extra spaces until we are at want_vcol
	    while (vcol < want_vcol)
	    {
		// Remember the first char we inserted
		if (curwin->w_cursor.lnum == Insstart_orig.lnum
				   && curwin->w_cursor.col < Insstart_orig.col)
		    Insstart_orig.col = curwin->w_cursor.col;

		if (State & VREPLACE_FLAG)
		    ins_char(' ');
		else
		{
		    ins_str((char_u *)" ");
		    if ((State & REPLACE_FLAG))
			replace_push(NUL);
		}
		getvcol(curwin, &curwin->w_cursor, &vcol, NULL, NULL);
	    }

	    // If we are now back where we started delete one character.  Can
	    // happen when using 'sts' and 'linebreak'.
	    if (vcol >= start_vcol)
		ins_bs_one(&vcol);
	}

	/*
	 * Delete up to starting point, start of line or previous word.
	 */
	else
	{
	    int cclass = 0, prev_cclass = 0;

	    if (has_mbyte)
		cclass = mb_get_class(ml_get_cursor());
	    do
	    {
#ifdef FEAT_RIGHTLEFT
		if (!revins_on) // put cursor on char to be deleted
#endif
		    dec_cursor();

		cc = gchar_cursor();
		// look multi-byte character class
		if (has_mbyte)
		{
		    prev_cclass = cclass;
		    cclass = mb_get_class(ml_get_cursor());
		}

		// start of word?
		if (mode == BACKSPACE_WORD && !vim_isspace(cc))
		{
		    mode = BACKSPACE_WORD_NOT_SPACE;
		    temp = vim_iswordc(cc);
		}
		// end of word?
		else if (mode == BACKSPACE_WORD_NOT_SPACE
			&& ((vim_isspace(cc) || vim_iswordc(cc) != temp)
			|| prev_cclass != cclass))
		{
#ifdef FEAT_RIGHTLEFT
		    if (!revins_on)
#endif
			inc_cursor();
#ifdef FEAT_RIGHTLEFT
		    else if (State & REPLACE_FLAG)
			dec_cursor();
#endif
		    break;
		}
		if (State & REPLACE_FLAG)
		    replace_do_bs(-1);
		else
		{
		    if (enc_utf8 && p_deco)
			(void)utfc_ptr2char(ml_get_cursor(), cpc);
		    (void)del_char(FALSE);
		    /*
		     * If there are combining characters and 'delcombine' is set
		     * move the cursor back.  Don't back up before the base
		     * character.
		     */
		    if (enc_utf8 && p_deco && cpc[0] != NUL)
			inc_cursor();
#ifdef FEAT_RIGHTLEFT
		    if (revins_chars)
		    {
			revins_chars--;
			revins_legal++;
		    }
		    if (revins_on && gchar_cursor() == NUL)
			break;
#endif
		}
		// Just a single backspace?:
		if (mode == BACKSPACE_CHAR)
		    break;
	    } while (
#ifdef FEAT_RIGHTLEFT
		    revins_on ||
#endif
		    (curwin->w_cursor.col > mincol
		    &&  (can_bs(BS_NOSTOP)
			|| (curwin->w_cursor.lnum != Insstart_orig.lnum
			|| curwin->w_cursor.col != Insstart_orig.col)
		    )));
	}
	did_backspace = TRUE;
    }
    did_si = FALSE;
    can_si = FALSE;
    can_si_back = FALSE;
    if (curwin->w_cursor.col <= 1)
	did_ai = FALSE;

    if (call_fix_indent)
	fix_indent();

    /*
     * It's a little strange to put backspaces into the redo
     * buffer, but it makes auto-indent a lot easier to deal
     * with.
     */
    AppendCharToRedobuff(c);

    // If deleted before the insertion point, adjust it
    if (curwin->w_cursor.lnum == Insstart_orig.lnum
				  && curwin->w_cursor.col < Insstart_orig.col)
	Insstart_orig.col = curwin->w_cursor.col;

    // vi behaviour: the cursor moves backward but the character that
    //		     was there remains visible
    // Vim behaviour: the cursor moves backward and the character that
    //		      was there is erased from the screen.
    // We can emulate the vi behaviour by pretending there is a dollar
    // displayed even when there isn't.
    //  --pkv Sun Jan 19 01:56:40 EST 2003
    if (vim_strchr(p_cpo, CPO_BACKSPACE) != NULL && dollar_vcol == -1)
	dollar_vcol = curwin->w_virtcol;

#ifdef FEAT_FOLDING
    // When deleting a char the cursor line must never be in a closed fold.
    // E.g., when 'foldmethod' is indent and deleting the first non-white
    // char before a Tab.
    if (did_backspace)
	foldOpenCursor();
#endif

    return did_backspace;
}