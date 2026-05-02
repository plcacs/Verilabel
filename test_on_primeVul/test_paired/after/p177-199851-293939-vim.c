ex_retab(exarg_T *eap)
{
    linenr_T	lnum;
    int		got_tab = FALSE;
    long	num_spaces = 0;
    long	num_tabs;
    long	len;
    long	col;
    long	vcol;
    long	start_col = 0;		// For start of white-space string
    long	start_vcol = 0;		// For start of white-space string
    long	old_len;
    char_u	*ptr;
    char_u	*new_line = (char_u *)1; // init to non-NULL
    int		did_undo;		// called u_save for current line
#ifdef FEAT_VARTABS
    int		*new_vts_array = NULL;
    char_u	*new_ts_str;		// string value of tab argument
#else
    int		temp;
    int		new_ts;
#endif
    int		save_list;
    linenr_T	first_line = 0;		// first changed line
    linenr_T	last_line = 0;		// last changed line

    save_list = curwin->w_p_list;
    curwin->w_p_list = 0;	    // don't want list mode here

#ifdef FEAT_VARTABS
    new_ts_str = eap->arg;
    if (tabstop_set(eap->arg, &new_vts_array) == FAIL)
	return;
    while (vim_isdigit(*(eap->arg)) || *(eap->arg) == ',')
	++(eap->arg);

    // This ensures that either new_vts_array and new_ts_str are freshly
    // allocated, or new_vts_array points to an existing array and new_ts_str
    // is null.
    if (new_vts_array == NULL)
    {
	new_vts_array = curbuf->b_p_vts_array;
	new_ts_str = NULL;
    }
    else
	new_ts_str = vim_strnsave(new_ts_str, eap->arg - new_ts_str);
#else
    ptr = eap->arg;
    new_ts = getdigits(&ptr);
    if (new_ts < 0 && *eap->arg == '-')
    {
	emsg(_(e_argument_must_be_positive));
	return;
    }
    if (new_ts < 0 || new_ts > TABSTOP_MAX)
    {
	semsg(_(e_invalid_argument_str), eap->arg);
	return;
    }
    if (new_ts == 0)
	new_ts = curbuf->b_p_ts;
#endif
    for (lnum = eap->line1; !got_int && lnum <= eap->line2; ++lnum)
    {
	ptr = ml_get(lnum);
	col = 0;
	vcol = 0;
	did_undo = FALSE;
	for (;;)
	{
	    if (VIM_ISWHITE(ptr[col]))
	    {
		if (!got_tab && num_spaces == 0)
		{
		    // First consecutive white-space
		    start_vcol = vcol;
		    start_col = col;
		}
		if (ptr[col] == ' ')
		    num_spaces++;
		else
		    got_tab = TRUE;
	    }
	    else
	    {
		if (got_tab || (eap->forceit && num_spaces > 1))
		{
		    // Retabulate this string of white-space

		    // len is virtual length of white string
		    len = num_spaces = vcol - start_vcol;
		    num_tabs = 0;
		    if (!curbuf->b_p_et)
		    {
#ifdef FEAT_VARTABS
			int t, s;

			tabstop_fromto(start_vcol, vcol,
					curbuf->b_p_ts, new_vts_array, &t, &s);
			num_tabs = t;
			num_spaces = s;
#else
			temp = new_ts - (start_vcol % new_ts);
			if (num_spaces >= temp)
			{
			    num_spaces -= temp;
			    num_tabs++;
			}
			num_tabs += num_spaces / new_ts;
			num_spaces -= (num_spaces / new_ts) * new_ts;
#endif
		    }
		    if (curbuf->b_p_et || got_tab ||
					(num_spaces + num_tabs < len))
		    {
			if (did_undo == FALSE)
			{
			    did_undo = TRUE;
			    if (u_save((linenr_T)(lnum - 1),
						(linenr_T)(lnum + 1)) == FAIL)
			    {
				new_line = NULL;	// flag out-of-memory
				break;
			    }
			}

			// len is actual number of white characters used
			len = num_spaces + num_tabs;
			old_len = (long)STRLEN(ptr);
			new_line = alloc(old_len - col + start_col + len + 1);
			if (new_line == NULL)
			    break;
			if (start_col > 0)
			    mch_memmove(new_line, ptr, (size_t)start_col);
			mch_memmove(new_line + start_col + len,
				      ptr + col, (size_t)(old_len - col + 1));
			ptr = new_line + start_col;
			for (col = 0; col < len; col++)
			    ptr[col] = (col < num_tabs) ? '\t' : ' ';
			if (ml_replace(lnum, new_line, FALSE) == OK)
			    // "new_line" may have been copied
			    new_line = curbuf->b_ml.ml_line_ptr;
			if (first_line == 0)
			    first_line = lnum;
			last_line = lnum;
			ptr = new_line;
			col = start_col + len;
		    }
		}
		got_tab = FALSE;
		num_spaces = 0;
	    }
	    if (ptr[col] == NUL)
		break;
	    vcol += chartabsize(ptr + col, (colnr_T)vcol);
	    if (vcol >= MAXCOL)
	    {
		emsg(_(e_resulting_text_too_long));
		break;
	    }
	    if (has_mbyte)
		col += (*mb_ptr2len)(ptr + col);
	    else
		++col;
	}
	if (new_line == NULL)		    // out of memory
	    break;
	line_breakcheck();
    }
    if (got_int)
	emsg(_(e_interrupted));

#ifdef FEAT_VARTABS
    // If a single value was given then it can be considered equal to
    // either the value of 'tabstop' or the value of 'vartabstop'.
    if (tabstop_count(curbuf->b_p_vts_array) == 0
	&& tabstop_count(new_vts_array) == 1
	&& curbuf->b_p_ts == tabstop_first(new_vts_array))
	; // not changed
    else if (tabstop_count(curbuf->b_p_vts_array) > 0
        && tabstop_eq(curbuf->b_p_vts_array, new_vts_array))
	; // not changed
    else
	redraw_curbuf_later(NOT_VALID);
#else
    if (curbuf->b_p_ts != new_ts)
	redraw_curbuf_later(NOT_VALID);
#endif
    if (first_line != 0)
	changed_lines(first_line, 0, last_line + 1, 0L);

    curwin->w_p_list = save_list;	// restore 'list'

#ifdef FEAT_VARTABS
    if (new_ts_str != NULL)		// set the new tabstop
    {
	// If 'vartabstop' is in use or if the value given to retab has more
	// than one tabstop then update 'vartabstop'.
	int *old_vts_ary = curbuf->b_p_vts_array;

	if (tabstop_count(old_vts_ary) > 0 || tabstop_count(new_vts_array) > 1)
	{
	    set_string_option_direct((char_u *)"vts", -1, new_ts_str,
							OPT_FREE|OPT_LOCAL, 0);
	    curbuf->b_p_vts_array = new_vts_array;
	    vim_free(old_vts_ary);
	}
	else
	{
	    // 'vartabstop' wasn't in use and a single value was given to
	    // retab then update 'tabstop'.
	    curbuf->b_p_ts = tabstop_first(new_vts_array);
	    vim_free(new_vts_array);
	}
	vim_free(new_ts_str);
    }
#else
    curbuf->b_p_ts = new_ts;
#endif
    coladvance(curwin->w_curswant);

    u_clearline();
}