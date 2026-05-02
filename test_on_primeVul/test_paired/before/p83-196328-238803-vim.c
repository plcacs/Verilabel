find_pattern_in_path(
    char_u	*ptr,		// pointer to search pattern
    int		dir UNUSED,	// direction of expansion
    int		len,		// length of search pattern
    int		whole,		// match whole words only
    int		skip_comments,	// don't match inside comments
    int		type,		// Type of search; are we looking for a type?
				// a macro?
    long	count,
    int		action,		// What to do when we find it
    linenr_T	start_lnum,	// first line to start searching
    linenr_T	end_lnum)	// last line for searching
{
    SearchedFile *files;		// Stack of included files
    SearchedFile *bigger;		// When we need more space
    int		max_path_depth = 50;
    long	match_count = 1;

    char_u	*pat;
    char_u	*new_fname;
    char_u	*curr_fname = curbuf->b_fname;
    char_u	*prev_fname = NULL;
    linenr_T	lnum;
    int		depth;
    int		depth_displayed;	// For type==CHECK_PATH
    int		old_files;
    int		already_searched;
    char_u	*file_line;
    char_u	*line;
    char_u	*p;
    char_u	save_char;
    int		define_matched;
    regmatch_T	regmatch;
    regmatch_T	incl_regmatch;
    regmatch_T	def_regmatch;
    int		matched = FALSE;
    int		did_show = FALSE;
    int		found = FALSE;
    int		i;
    char_u	*already = NULL;
    char_u	*startp = NULL;
    char_u	*inc_opt = NULL;
#if defined(FEAT_QUICKFIX)
    win_T	*curwin_save = NULL;
#endif

    regmatch.regprog = NULL;
    incl_regmatch.regprog = NULL;
    def_regmatch.regprog = NULL;

    file_line = alloc(LSIZE);
    if (file_line == NULL)
	return;

    if (type != CHECK_PATH && type != FIND_DEFINE
	    // when CONT_SOL is set compare "ptr" with the beginning of the
	    // line is faster than quote_meta/regcomp/regexec "ptr" -- Acevedo
	    && !compl_status_sol())
    {
	pat = alloc(len + 5);
	if (pat == NULL)
	    goto fpip_end;
	sprintf((char *)pat, whole ? "\\<%.*s\\>" : "%.*s", len, ptr);
	// ignore case according to p_ic, p_scs and pat
	regmatch.rm_ic = ignorecase(pat);
	regmatch.regprog = vim_regcomp(pat, magic_isset() ? RE_MAGIC : 0);
	vim_free(pat);
	if (regmatch.regprog == NULL)
	    goto fpip_end;
    }
    inc_opt = (*curbuf->b_p_inc == NUL) ? p_inc : curbuf->b_p_inc;
    if (*inc_opt != NUL)
    {
	incl_regmatch.regprog = vim_regcomp(inc_opt,
						 magic_isset() ? RE_MAGIC : 0);
	if (incl_regmatch.regprog == NULL)
	    goto fpip_end;
	incl_regmatch.rm_ic = FALSE;	// don't ignore case in incl. pat.
    }
    if (type == FIND_DEFINE && (*curbuf->b_p_def != NUL || *p_def != NUL))
    {
	def_regmatch.regprog = vim_regcomp(*curbuf->b_p_def == NUL
			   ? p_def : curbuf->b_p_def,
						 magic_isset() ? RE_MAGIC : 0);
	if (def_regmatch.regprog == NULL)
	    goto fpip_end;
	def_regmatch.rm_ic = FALSE;	// don't ignore case in define pat.
    }
    files = lalloc_clear(max_path_depth * sizeof(SearchedFile), TRUE);
    if (files == NULL)
	goto fpip_end;
    old_files = max_path_depth;
    depth = depth_displayed = -1;

    lnum = start_lnum;
    if (end_lnum > curbuf->b_ml.ml_line_count)
	end_lnum = curbuf->b_ml.ml_line_count;
    if (lnum > end_lnum)		// do at least one line
	lnum = end_lnum;
    line = ml_get(lnum);

    for (;;)
    {
	if (incl_regmatch.regprog != NULL
		&& vim_regexec(&incl_regmatch, line, (colnr_T)0))
	{
	    char_u *p_fname = (curr_fname == curbuf->b_fname)
					      ? curbuf->b_ffname : curr_fname;

	    if (inc_opt != NULL && strstr((char *)inc_opt, "\\zs") != NULL)
		// Use text from '\zs' to '\ze' (or end) of 'include'.
		new_fname = find_file_name_in_path(incl_regmatch.startp[0],
		       (int)(incl_regmatch.endp[0] - incl_regmatch.startp[0]),
				 FNAME_EXP|FNAME_INCL|FNAME_REL, 1L, p_fname);
	    else
		// Use text after match with 'include'.
		new_fname = file_name_in_line(incl_regmatch.endp[0], 0,
			     FNAME_EXP|FNAME_INCL|FNAME_REL, 1L, p_fname, NULL);
	    already_searched = FALSE;
	    if (new_fname != NULL)
	    {
		// Check whether we have already searched in this file
		for (i = 0;; i++)
		{
		    if (i == depth + 1)
			i = old_files;
		    if (i == max_path_depth)
			break;
		    if (fullpathcmp(new_fname, files[i].name, TRUE, TRUE)
								    & FPC_SAME)
		    {
			if (type != CHECK_PATH
				&& action == ACTION_SHOW_ALL
				&& files[i].matched)
			{
			    msg_putchar('\n');	    // cursor below last one
			    if (!got_int)	    // don't display if 'q'
						    // typed at "--more--"
						    // message
			    {
				msg_home_replace_hl(new_fname);
				msg_puts(_(" (includes previously listed match)"));
				prev_fname = NULL;
			    }
			}
			VIM_CLEAR(new_fname);
			already_searched = TRUE;
			break;
		    }
		}
	    }

	    if (type == CHECK_PATH && (action == ACTION_SHOW_ALL
				 || (new_fname == NULL && !already_searched)))
	    {
		if (did_show)
		    msg_putchar('\n');	    // cursor below last one
		else
		{
		    gotocmdline(TRUE);	    // cursor at status line
		    msg_puts_title(_("--- Included files "));
		    if (action != ACTION_SHOW_ALL)
			msg_puts_title(_("not found "));
		    msg_puts_title(_("in path ---\n"));
		}
		did_show = TRUE;
		while (depth_displayed < depth && !got_int)
		{
		    ++depth_displayed;
		    for (i = 0; i < depth_displayed; i++)
			msg_puts("  ");
		    msg_home_replace(files[depth_displayed].name);
		    msg_puts(" -->\n");
		}
		if (!got_int)		    // don't display if 'q' typed
					    // for "--more--" message
		{
		    for (i = 0; i <= depth_displayed; i++)
			msg_puts("  ");
		    if (new_fname != NULL)
		    {
			// using "new_fname" is more reliable, e.g., when
			// 'includeexpr' is set.
			msg_outtrans_attr(new_fname, HL_ATTR(HLF_D));
		    }
		    else
		    {
			/*
			 * Isolate the file name.
			 * Include the surrounding "" or <> if present.
			 */
			if (inc_opt != NULL
				   && strstr((char *)inc_opt, "\\zs") != NULL)
			{
			    // pattern contains \zs, use the match
			    p = incl_regmatch.startp[0];
			    i = (int)(incl_regmatch.endp[0]
						   - incl_regmatch.startp[0]);
			}
			else
			{
			    // find the file name after the end of the match
			    for (p = incl_regmatch.endp[0];
						  *p && !vim_isfilec(*p); p++)
				;
			    for (i = 0; vim_isfilec(p[i]); i++)
				;
			}

			if (i == 0)
			{
			    // Nothing found, use the rest of the line.
			    p = incl_regmatch.endp[0];
			    i = (int)STRLEN(p);
			}
			// Avoid checking before the start of the line, can
			// happen if \zs appears in the regexp.
			else if (p > line)
			{
			    if (p[-1] == '"' || p[-1] == '<')
			    {
				--p;
				++i;
			    }
			    if (p[i] == '"' || p[i] == '>')
				++i;
			}
			save_char = p[i];
			p[i] = NUL;
			msg_outtrans_attr(p, HL_ATTR(HLF_D));
			p[i] = save_char;
		    }

		    if (new_fname == NULL && action == ACTION_SHOW_ALL)
		    {
			if (already_searched)
			    msg_puts(_("  (Already listed)"));
			else
			    msg_puts(_("  NOT FOUND"));
		    }
		}
		out_flush();	    // output each line directly
	    }

	    if (new_fname != NULL)
	    {
		// Push the new file onto the file stack
		if (depth + 1 == old_files)
		{
		    bigger = ALLOC_MULT(SearchedFile, max_path_depth * 2);
		    if (bigger != NULL)
		    {
			for (i = 0; i <= depth; i++)
			    bigger[i] = files[i];
			for (i = depth + 1; i < old_files + max_path_depth; i++)
			{
			    bigger[i].fp = NULL;
			    bigger[i].name = NULL;
			    bigger[i].lnum = 0;
			    bigger[i].matched = FALSE;
			}
			for (i = old_files; i < max_path_depth; i++)
			    bigger[i + max_path_depth] = files[i];
			old_files += max_path_depth;
			max_path_depth *= 2;
			vim_free(files);
			files = bigger;
		    }
		}
		if ((files[depth + 1].fp = mch_fopen((char *)new_fname, "r"))
								    == NULL)
		    vim_free(new_fname);
		else
		{
		    if (++depth == old_files)
		    {
			/*
			 * lalloc() for 'bigger' must have failed above.  We
			 * will forget one of our already visited files now.
			 */
			vim_free(files[old_files].name);
			++old_files;
		    }
		    files[depth].name = curr_fname = new_fname;
		    files[depth].lnum = 0;
		    files[depth].matched = FALSE;
		    if (action == ACTION_EXPAND)
		    {
			msg_hist_off = TRUE;	// reset in msg_trunc_attr()
			vim_snprintf((char*)IObuff, IOSIZE,
				_("Scanning included file: %s"),
				(char *)new_fname);
			msg_trunc_attr((char *)IObuff, TRUE, HL_ATTR(HLF_R));
		    }
		    else if (p_verbose >= 5)
		    {
			verbose_enter();
			smsg(_("Searching included file %s"),
							   (char *)new_fname);
			verbose_leave();
		    }

		}
	    }
	}
	else
	{
	    /*
	     * Check if the line is a define (type == FIND_DEFINE)
	     */
	    p = line;
search_line:
	    define_matched = FALSE;
	    if (def_regmatch.regprog != NULL
			      && vim_regexec(&def_regmatch, line, (colnr_T)0))
	    {
		/*
		 * Pattern must be first identifier after 'define', so skip
		 * to that position before checking for match of pattern.  Also
		 * don't let it match beyond the end of this identifier.
		 */
		p = def_regmatch.endp[0];
		while (*p && !vim_iswordc(*p))
		    p++;
		define_matched = TRUE;
	    }

	    /*
	     * Look for a match.  Don't do this if we are looking for a
	     * define and this line didn't match define_prog above.
	     */
	    if (def_regmatch.regprog == NULL || define_matched)
	    {
		if (define_matched || compl_status_sol())
		{
		    // compare the first "len" chars from "ptr"
		    startp = skipwhite(p);
		    if (p_ic)
			matched = !MB_STRNICMP(startp, ptr, len);
		    else
			matched = !STRNCMP(startp, ptr, len);
		    if (matched && define_matched && whole
						  && vim_iswordc(startp[len]))
			matched = FALSE;
		}
		else if (regmatch.regprog != NULL
			 && vim_regexec(&regmatch, line, (colnr_T)(p - line)))
		{
		    matched = TRUE;
		    startp = regmatch.startp[0];
		    /*
		     * Check if the line is not a comment line (unless we are
		     * looking for a define).  A line starting with "# define"
		     * is not considered to be a comment line.
		     */
		    if (!define_matched && skip_comments)
		    {
			if ((*line != '#' ||
				STRNCMP(skipwhite(line + 1), "define", 6) != 0)
				&& get_leader_len(line, NULL, FALSE, TRUE))
			    matched = FALSE;

			/*
			 * Also check for a "/ *" or "/ /" before the match.
			 * Skips lines like "int backwards;  / * normal index
			 * * /" when looking for "normal".
			 * Note: Doesn't skip "/ *" in comments.
			 */
			p = skipwhite(line);
			if (matched
				|| (p[0] == '/' && p[1] == '*') || p[0] == '*')
			    for (p = line; *p && p < startp; ++p)
			    {
				if (matched
					&& p[0] == '/'
					&& (p[1] == '*' || p[1] == '/'))
				{
				    matched = FALSE;
				    // After "//" all text is comment
				    if (p[1] == '/')
					break;
				    ++p;
				}
				else if (!matched && p[0] == '*' && p[1] == '/')
				{
				    // Can find match after "* /".
				    matched = TRUE;
				    ++p;
				}
			    }
		    }
		}
	    }
	}
	if (matched)
	{
	    if (action == ACTION_EXPAND)
	    {
		int	cont_s_ipos = FALSE;
		int	add_r;
		char_u	*aux;

		if (depth == -1 && lnum == curwin->w_cursor.lnum)
		    break;
		found = TRUE;
		aux = p = startp;
		if (compl_status_adding())
		{
		    p += ins_compl_len();
		    if (vim_iswordp(p))
			goto exit_matched;
		    p = find_word_start(p);
		}
		p = find_word_end(p);
		i = (int)(p - aux);

		if (compl_status_adding() && i == ins_compl_len())
		{
		    // IOSIZE > compl_length, so the STRNCPY works
		    STRNCPY(IObuff, aux, i);

		    // Get the next line: when "depth" < 0  from the current
		    // buffer, otherwise from the included file.  Jump to
		    // exit_matched when past the last line.
		    if (depth < 0)
		    {
			if (lnum >= end_lnum)
			    goto exit_matched;
			line = ml_get(++lnum);
		    }
		    else if (vim_fgets(line = file_line,
						      LSIZE, files[depth].fp))
			goto exit_matched;

		    // we read a line, set "already" to check this "line" later
		    // if depth >= 0 we'll increase files[depth].lnum far
		    // below  -- Acevedo
		    already = aux = p = skipwhite(line);
		    p = find_word_start(p);
		    p = find_word_end(p);
		    if (p > aux)
		    {
			if (*aux != ')' && IObuff[i-1] != TAB)
			{
			    if (IObuff[i-1] != ' ')
				IObuff[i++] = ' ';
			    // IObuf =~ "\(\k\|\i\).* ", thus i >= 2
			    if (p_js
				&& (IObuff[i-2] == '.'
				    || (vim_strchr(p_cpo, CPO_JOINSP) == NULL
					&& (IObuff[i-2] == '?'
					    || IObuff[i-2] == '!'))))
				IObuff[i++] = ' ';
			}
			// copy as much as possible of the new word
			if (p - aux >= IOSIZE - i)
			    p = aux + IOSIZE - i - 1;
			STRNCPY(IObuff + i, aux, p - aux);
			i += (int)(p - aux);
			cont_s_ipos = TRUE;
		    }
		    IObuff[i] = NUL;
		    aux = IObuff;

		    if (i == ins_compl_len())
			goto exit_matched;
		}

		add_r = ins_compl_add_infercase(aux, i, p_ic,
			curr_fname == curbuf->b_fname ? NULL : curr_fname,
			dir, cont_s_ipos);
		if (add_r == OK)
		    // if dir was BACKWARD then honor it just once
		    dir = FORWARD;
		else if (add_r == FAIL)
		    break;
	    }
	    else if (action == ACTION_SHOW_ALL)
	    {
		found = TRUE;
		if (!did_show)
		    gotocmdline(TRUE);		// cursor at status line
		if (curr_fname != prev_fname)
		{
		    if (did_show)
			msg_putchar('\n');	// cursor below last one
		    if (!got_int)		// don't display if 'q' typed
						// at "--more--" message
			msg_home_replace_hl(curr_fname);
		    prev_fname = curr_fname;
		}
		did_show = TRUE;
		if (!got_int)
		    show_pat_in_path(line, type, TRUE, action,
			    (depth == -1) ? NULL : files[depth].fp,
			    (depth == -1) ? &lnum : &files[depth].lnum,
			    match_count++);

		// Set matched flag for this file and all the ones that
		// include it
		for (i = 0; i <= depth; ++i)
		    files[i].matched = TRUE;
	    }
	    else if (--count <= 0)
	    {
		found = TRUE;
		if (depth == -1 && lnum == curwin->w_cursor.lnum
#if defined(FEAT_QUICKFIX)
						      && g_do_tagpreview == 0
#endif
						      )
		    emsg(_(e_match_is_on_current_line));
		else if (action == ACTION_SHOW)
		{
		    show_pat_in_path(line, type, did_show, action,
			(depth == -1) ? NULL : files[depth].fp,
			(depth == -1) ? &lnum : &files[depth].lnum, 1L);
		    did_show = TRUE;
		}
		else
		{
#ifdef FEAT_GUI
		    need_mouse_correct = TRUE;
#endif
#if defined(FEAT_QUICKFIX)
		    // ":psearch" uses the preview window
		    if (g_do_tagpreview != 0)
		    {
			curwin_save = curwin;
			prepare_tagpreview(TRUE, TRUE, FALSE);
		    }
#endif
		    if (action == ACTION_SPLIT)
		    {
			if (win_split(0, 0) == FAIL)
			    break;
			RESET_BINDING(curwin);
		    }
		    if (depth == -1)
		    {
			// match in current file
#if defined(FEAT_QUICKFIX)
			if (g_do_tagpreview != 0)
			{
			    if (!win_valid(curwin_save))
				break;
			    if (!GETFILE_SUCCESS(getfile(
					   curwin_save->w_buffer->b_fnum, NULL,
						     NULL, TRUE, lnum, FALSE)))
				break;	// failed to jump to file
			}
			else
#endif
			    setpcmark();
			curwin->w_cursor.lnum = lnum;
			check_cursor();
		    }
		    else
		    {
			if (!GETFILE_SUCCESS(getfile(
					0, files[depth].name, NULL, TRUE,
						    files[depth].lnum, FALSE)))
			    break;	// failed to jump to file
			// autocommands may have changed the lnum, we don't
			// want that here
			curwin->w_cursor.lnum = files[depth].lnum;
		    }
		}
		if (action != ACTION_SHOW)
		{
		    curwin->w_cursor.col = (colnr_T)(startp - line);
		    curwin->w_set_curswant = TRUE;
		}

#if defined(FEAT_QUICKFIX)
		if (g_do_tagpreview != 0
			   && curwin != curwin_save && win_valid(curwin_save))
		{
		    // Return cursor to where we were
		    validate_cursor();
		    redraw_later(VALID);
		    win_enter(curwin_save, TRUE);
		}
# ifdef FEAT_PROP_POPUP
		else if (WIN_IS_POPUP(curwin))
		    // can't keep focus in popup window
		    win_enter(firstwin, TRUE);
# endif
#endif
		break;
	    }
exit_matched:
	    matched = FALSE;
	    // look for other matches in the rest of the line if we
	    // are not at the end of it already
	    if (def_regmatch.regprog == NULL
		    && action == ACTION_EXPAND
		    && !compl_status_sol()
		    && *startp != NUL
		    && *(p = startp + mb_ptr2len(startp)) != NUL)
		goto search_line;
	}
	line_breakcheck();
	if (action == ACTION_EXPAND)
	    ins_compl_check_keys(30, FALSE);
	if (got_int || ins_compl_interrupted())
	    break;

	/*
	 * Read the next line.  When reading an included file and encountering
	 * end-of-file, close the file and continue in the file that included
	 * it.
	 */
	while (depth >= 0 && !already
		&& vim_fgets(line = file_line, LSIZE, files[depth].fp))
	{
	    fclose(files[depth].fp);
	    --old_files;
	    files[old_files].name = files[depth].name;
	    files[old_files].matched = files[depth].matched;
	    --depth;
	    curr_fname = (depth == -1) ? curbuf->b_fname
				       : files[depth].name;
	    if (depth < depth_displayed)
		depth_displayed = depth;
	}
	if (depth >= 0)		// we could read the line
	{
	    files[depth].lnum++;
	    // Remove any CR and LF from the line.
	    i = (int)STRLEN(line);
	    if (i > 0 && line[i - 1] == '\n')
		line[--i] = NUL;
	    if (i > 0 && line[i - 1] == '\r')
		line[--i] = NUL;
	}
	else if (!already)
	{
	    if (++lnum > end_lnum)
		break;
	    line = ml_get(lnum);
	}
	already = NULL;
    }
    // End of big for (;;) loop.

    // Close any files that are still open.
    for (i = 0; i <= depth; i++)
    {
	fclose(files[i].fp);
	vim_free(files[i].name);
    }
    for (i = old_files; i < max_path_depth; i++)
	vim_free(files[i].name);
    vim_free(files);

    if (type == CHECK_PATH)
    {
	if (!did_show)
	{
	    if (action != ACTION_SHOW_ALL)
		msg(_("All included files were found"));
	    else
		msg(_("No included files"));
	}
    }
    else if (!found && action != ACTION_EXPAND)
    {
	if (got_int || ins_compl_interrupted())
	    emsg(_(e_interrupted));
	else if (type == FIND_DEFINE)
	    emsg(_(e_couldnt_find_definition));
	else
	    emsg(_(e_couldnt_find_pattern));
    }
    if (action == ACTION_SHOW || action == ACTION_SHOW_ALL)
	msg_end();

fpip_end:
    vim_free(file_line);
    vim_regfree(regmatch.regprog);
    vim_regfree(incl_regmatch.regprog);
    vim_regfree(def_regmatch.regprog);
}