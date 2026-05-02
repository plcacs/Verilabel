do_tag(
    char_u	*tag,		// tag (pattern) to jump to
    int		type,
    int		count,
    int		forceit,	// :ta with !
    int		verbose)	// print "tag not found" message
{
    taggy_T	*tagstack = curwin->w_tagstack;
    int		tagstackidx = curwin->w_tagstackidx;
    int		tagstacklen = curwin->w_tagstacklen;
    int		cur_match = 0;
    int		cur_fnum = curbuf->b_fnum;
    int		oldtagstackidx = tagstackidx;
    int		prevtagstackidx = tagstackidx;
    int		prev_num_matches;
    int		new_tag = FALSE;
    int		i;
    int		ic;
    int		no_regexp = FALSE;
    int		error_cur_match = 0;
    int		save_pos = FALSE;
    fmark_T	saved_fmark;
#ifdef FEAT_CSCOPE
    int		jumped_to_tag = FALSE;
#endif
    int		new_num_matches;
    char_u	**new_matches;
    int		use_tagstack;
    int		skip_msg = FALSE;
    char_u	*buf_ffname = curbuf->b_ffname;	    // name to use for
						    // priority computation
    int		use_tfu = 1;
    char_u	*tofree = NULL;

    // remember the matches for the last used tag
    static int		num_matches = 0;
    static int		max_num_matches = 0;  // limit used for match search
    static char_u	**matches = NULL;
    static int		flags;

#ifdef FEAT_EVAL
    if (tfu_in_use)
    {
	emsg(_(e_cannot_modify_tag_stack_within_tagfunc));
	return FALSE;
    }
#endif

#ifdef EXITFREE
    if (type == DT_FREE)
    {
	// remove the list of matches
	FreeWild(num_matches, matches);
# ifdef FEAT_CSCOPE
	cs_free_tags();
# endif
	num_matches = 0;
	return FALSE;
    }
#endif

    if (type == DT_HELP)
    {
	type = DT_TAG;
	no_regexp = TRUE;
	use_tfu = 0;
    }

    prev_num_matches = num_matches;
    free_string_option(nofile_fname);
    nofile_fname = NULL;

    CLEAR_POS(&saved_fmark.mark);	// shutup gcc 4.0
    saved_fmark.fnum = 0;

    /*
     * Don't add a tag to the tagstack if 'tagstack' has been reset.
     */
    if ((!p_tgst && *tag != NUL))
    {
	use_tagstack = FALSE;
	new_tag = TRUE;
#if defined(FEAT_QUICKFIX)
	if (g_do_tagpreview != 0)
	{
	    tagstack_clear_entry(&ptag_entry);
	    if ((ptag_entry.tagname = vim_strsave(tag)) == NULL)
		goto end_do_tag;
	}
#endif
    }
    else
    {
#if defined(FEAT_QUICKFIX)
	if (g_do_tagpreview != 0)
	    use_tagstack = FALSE;
	else
#endif
	    use_tagstack = TRUE;

	// new pattern, add to the tag stack
	if (*tag != NUL
		&& (type == DT_TAG || type == DT_SELECT || type == DT_JUMP
#ifdef FEAT_QUICKFIX
		    || type == DT_LTAG
#endif
#ifdef FEAT_CSCOPE
		    || type == DT_CSCOPE
#endif
		    ))
	{
#if defined(FEAT_QUICKFIX)
	    if (g_do_tagpreview != 0)
	    {
		if (ptag_entry.tagname != NULL
			&& STRCMP(ptag_entry.tagname, tag) == 0)
		{
		    // Jumping to same tag: keep the current match, so that
		    // the CursorHold autocommand example works.
		    cur_match = ptag_entry.cur_match;
		    cur_fnum = ptag_entry.cur_fnum;
		}
		else
		{
		    tagstack_clear_entry(&ptag_entry);
		    if ((ptag_entry.tagname = vim_strsave(tag)) == NULL)
			goto end_do_tag;
		}
	    }
	    else
#endif
	    {
		/*
		 * If the last used entry is not at the top, delete all tag
		 * stack entries above it.
		 */
		while (tagstackidx < tagstacklen)
		    tagstack_clear_entry(&tagstack[--tagstacklen]);

		// if the tagstack is full: remove oldest entry
		if (++tagstacklen > TAGSTACKSIZE)
		{
		    tagstacklen = TAGSTACKSIZE;
		    tagstack_clear_entry(&tagstack[0]);
		    for (i = 1; i < tagstacklen; ++i)
			tagstack[i - 1] = tagstack[i];
		    --tagstackidx;
		}

		/*
		 * put the tag name in the tag stack
		 */
		if ((tagstack[tagstackidx].tagname = vim_strsave(tag)) == NULL)
		{
		    curwin->w_tagstacklen = tagstacklen - 1;
		    goto end_do_tag;
		}
		curwin->w_tagstacklen = tagstacklen;

		save_pos = TRUE;	// save the cursor position below
	    }

	    new_tag = TRUE;
	}
	else
	{
	    if (
#if defined(FEAT_QUICKFIX)
		    g_do_tagpreview != 0 ? ptag_entry.tagname == NULL :
#endif
		    tagstacklen == 0)
	    {
		// empty stack
		emsg(_(e_tag_stack_empty));
		goto end_do_tag;
	    }

	    if (type == DT_POP)		// go to older position
	    {
#ifdef FEAT_FOLDING
		int	old_KeyTyped = KeyTyped;
#endif
		if ((tagstackidx -= count) < 0)
		{
		    emsg(_(e_at_bottom_of_tag_stack));
		    if (tagstackidx + count == 0)
		    {
			// We did [num]^T from the bottom of the stack
			tagstackidx = 0;
			goto end_do_tag;
		    }
		    // We weren't at the bottom of the stack, so jump all the
		    // way to the bottom now.
		    tagstackidx = 0;
		}
		else if (tagstackidx >= tagstacklen)    // count == 0?
		{
		    emsg(_(e_at_top_of_tag_stack));
		    goto end_do_tag;
		}

		// Make a copy of the fmark, autocommands may invalidate the
		// tagstack before it's used.
		saved_fmark = tagstack[tagstackidx].fmark;
		if (saved_fmark.fnum != curbuf->b_fnum)
		{
		    /*
		     * Jump to other file. If this fails (e.g. because the
		     * file was changed) keep original position in tag stack.
		     */
		    if (buflist_getfile(saved_fmark.fnum, saved_fmark.mark.lnum,
					       GETF_SETMARK, forceit) == FAIL)
		    {
			tagstackidx = oldtagstackidx;  // back to old posn
			goto end_do_tag;
		    }
		    // An BufReadPost autocommand may jump to the '" mark, but
		    // we don't what that here.
		    curwin->w_cursor.lnum = saved_fmark.mark.lnum;
		}
		else
		{
		    setpcmark();
		    curwin->w_cursor.lnum = saved_fmark.mark.lnum;
		}
		curwin->w_cursor.col = saved_fmark.mark.col;
		curwin->w_set_curswant = TRUE;
		check_cursor();
#ifdef FEAT_FOLDING
		if ((fdo_flags & FDO_TAG) && old_KeyTyped)
		    foldOpenCursor();
#endif

		// remove the old list of matches
		FreeWild(num_matches, matches);
#ifdef FEAT_CSCOPE
		cs_free_tags();
#endif
		num_matches = 0;
		tag_freematch();
		goto end_do_tag;
	    }

	    if (type == DT_TAG
#if defined(FEAT_QUICKFIX)
		    || type == DT_LTAG
#endif
	       )
	    {
#if defined(FEAT_QUICKFIX)
		if (g_do_tagpreview != 0)
		{
		    cur_match = ptag_entry.cur_match;
		    cur_fnum = ptag_entry.cur_fnum;
		}
		else
#endif
		{
		    // ":tag" (no argument): go to newer pattern
		    save_pos = TRUE;	// save the cursor position below
		    if ((tagstackidx += count - 1) >= tagstacklen)
		    {
			/*
			 * Beyond the last one, just give an error message and
			 * go to the last one.  Don't store the cursor
			 * position.
			 */
			tagstackidx = tagstacklen - 1;
			emsg(_(e_at_top_of_tag_stack));
			save_pos = FALSE;
		    }
		    else if (tagstackidx < 0)	// must have been count == 0
		    {
			emsg(_(e_at_bottom_of_tag_stack));
			tagstackidx = 0;
			goto end_do_tag;
		    }
		    cur_match = tagstack[tagstackidx].cur_match;
		    cur_fnum = tagstack[tagstackidx].cur_fnum;
		}
		new_tag = TRUE;
	    }
	    else				// go to other matching tag
	    {
		// Save index for when selection is cancelled.
		prevtagstackidx = tagstackidx;

#if defined(FEAT_QUICKFIX)
		if (g_do_tagpreview != 0)
		{
		    cur_match = ptag_entry.cur_match;
		    cur_fnum = ptag_entry.cur_fnum;
		}
		else
#endif
		{
		    if (--tagstackidx < 0)
			tagstackidx = 0;
		    cur_match = tagstack[tagstackidx].cur_match;
		    cur_fnum = tagstack[tagstackidx].cur_fnum;
		}
		switch (type)
		{
		    case DT_FIRST: cur_match = count - 1; break;
		    case DT_SELECT:
		    case DT_JUMP:
#ifdef FEAT_CSCOPE
		    case DT_CSCOPE:
#endif
		    case DT_LAST:  cur_match = MAXCOL - 1; break;
		    case DT_NEXT:  cur_match += count; break;
		    case DT_PREV:  cur_match -= count; break;
		}
		if (cur_match >= MAXCOL)
		    cur_match = MAXCOL - 1;
		else if (cur_match < 0)
		{
		    emsg(_(e_cannot_go_before_first_matching_tag));
		    skip_msg = TRUE;
		    cur_match = 0;
		    cur_fnum = curbuf->b_fnum;
		}
	    }
	}

#if defined(FEAT_QUICKFIX)
	if (g_do_tagpreview != 0)
	{
	    if (type != DT_SELECT && type != DT_JUMP)
	    {
		ptag_entry.cur_match = cur_match;
		ptag_entry.cur_fnum = cur_fnum;
	    }
	}
	else
#endif
	{
	    /*
	     * For ":tag [arg]" or ":tselect" remember position before the jump.
	     */
	    saved_fmark = tagstack[tagstackidx].fmark;
	    if (save_pos)
	    {
		tagstack[tagstackidx].fmark.mark = curwin->w_cursor;
		tagstack[tagstackidx].fmark.fnum = curbuf->b_fnum;
	    }

	    // Curwin will change in the call to jumpto_tag() if ":stag" was
	    // used or an autocommand jumps to another window; store value of
	    // tagstackidx now.
	    curwin->w_tagstackidx = tagstackidx;
	    if (type != DT_SELECT && type != DT_JUMP)
	    {
		curwin->w_tagstack[tagstackidx].cur_match = cur_match;
		curwin->w_tagstack[tagstackidx].cur_fnum = cur_fnum;
	    }
	}
    }

    // When not using the current buffer get the name of buffer "cur_fnum".
    // Makes sure that the tag order doesn't change when using a remembered
    // position for "cur_match".
    if (cur_fnum != curbuf->b_fnum)
    {
	buf_T *buf = buflist_findnr(cur_fnum);

	if (buf != NULL)
	    buf_ffname = buf->b_ffname;
    }

    /*
     * Repeat searching for tags, when a file has not been found.
     */
    for (;;)
    {
	int	other_name;
	char_u	*name;

	/*
	 * When desired match not found yet, try to find it (and others).
	 */
	if (use_tagstack)
	{
	    // make a copy, the tagstack may change in 'tagfunc'
	    name = vim_strsave(tagstack[tagstackidx].tagname);
	    vim_free(tofree);
	    tofree = name;
	}
#if defined(FEAT_QUICKFIX)
	else if (g_do_tagpreview != 0)
	    name = ptag_entry.tagname;
#endif
	else
	    name = tag;
	other_name = (tagmatchname == NULL || STRCMP(tagmatchname, name) != 0);
	if (new_tag
		|| (cur_match >= num_matches && max_num_matches != MAXCOL)
		|| other_name)
	{
	    if (other_name)
	    {
		vim_free(tagmatchname);
		tagmatchname = vim_strsave(name);
	    }

	    if (type == DT_SELECT || type == DT_JUMP
#if defined(FEAT_QUICKFIX)
		|| type == DT_LTAG
#endif
		)
		cur_match = MAXCOL - 1;
	    if (type == DT_TAG)
		max_num_matches = MAXCOL;
	    else
		max_num_matches = cur_match + 1;

	    // when the argument starts with '/', use it as a regexp
	    if (!no_regexp && *name == '/')
	    {
		flags = TAG_REGEXP;
		++name;
	    }
	    else
		flags = TAG_NOIC;

#ifdef FEAT_CSCOPE
	    if (type == DT_CSCOPE)
		flags = TAG_CSCOPE;
#endif
	    if (verbose)
		flags |= TAG_VERBOSE;

	    if (!use_tfu)
		flags |= TAG_NO_TAGFUNC;

	    if (find_tags(name, &new_num_matches, &new_matches, flags,
					    max_num_matches, buf_ffname) == OK
		    && new_num_matches < max_num_matches)
		max_num_matches = MAXCOL; // If less than max_num_matches
					  // found: all matches found.

	    // If there already were some matches for the same name, move them
	    // to the start.  Avoids that the order changes when using
	    // ":tnext" and jumping to another file.
	    if (!new_tag && !other_name)
	    {
		int	    j, k;
		int	    idx = 0;
		tagptrs_T   tagp, tagp2;

		// Find the position of each old match in the new list.  Need
		// to use parse_match() to find the tag line.
		for (j = 0; j < num_matches; ++j)
		{
		    parse_match(matches[j], &tagp);
		    for (i = idx; i < new_num_matches; ++i)
		    {
			parse_match(new_matches[i], &tagp2);
			if (STRCMP(tagp.tagname, tagp2.tagname) == 0)
			{
			    char_u *p = new_matches[i];
			    for (k = i; k > idx; --k)
				new_matches[k] = new_matches[k - 1];
			    new_matches[idx++] = p;
			    break;
			}
		    }
		}
	    }
	    FreeWild(num_matches, matches);
	    num_matches = new_num_matches;
	    matches = new_matches;
	}

	if (num_matches <= 0)
	{
	    if (verbose)
		semsg(_(e_tag_not_found_str), name);
#if defined(FEAT_QUICKFIX)
	    g_do_tagpreview = 0;
#endif
	}
	else
	{
	    int ask_for_selection = FALSE;

#ifdef FEAT_CSCOPE
	    if (type == DT_CSCOPE && num_matches > 1)
	    {
		cs_print_tags();
		ask_for_selection = TRUE;
	    }
	    else
#endif
	    if (type == DT_TAG && *tag != NUL)
		// If a count is supplied to the ":tag <name>" command, then
		// jump to count'th matching tag.
		cur_match = count > 0 ? count - 1 : 0;
	    else if (type == DT_SELECT || (type == DT_JUMP && num_matches > 1))
	    {
		print_tag_list(new_tag, use_tagstack, num_matches, matches);
		ask_for_selection = TRUE;
	    }
#if defined(FEAT_QUICKFIX) && defined(FEAT_EVAL)
	    else if (type == DT_LTAG)
	    {
		if (add_llist_tags(tag, num_matches, matches) == FAIL)
		    goto end_do_tag;
		cur_match = 0;		// Jump to the first tag
	    }
#endif

	    if (ask_for_selection == TRUE)
	    {
		/*
		 * Ask to select a tag from the list.
		 */
		i = prompt_for_number(NULL);
		if (i <= 0 || i > num_matches || got_int)
		{
		    // no valid choice: don't change anything
		    if (use_tagstack)
		    {
			tagstack[tagstackidx].fmark = saved_fmark;
			tagstackidx = prevtagstackidx;
		    }
#ifdef FEAT_CSCOPE
		    cs_free_tags();
		    jumped_to_tag = TRUE;
#endif
		    break;
		}
		cur_match = i - 1;
	    }

	    if (cur_match >= num_matches)
	    {
		// Avoid giving this error when a file wasn't found and we're
		// looking for a match in another file, which wasn't found.
		// There will be an emsg("file doesn't exist") below then.
		if ((type == DT_NEXT || type == DT_FIRST)
						      && nofile_fname == NULL)
		{
		    if (num_matches == 1)
			emsg(_(e_there_is_only_one_matching_tag));
		    else
			emsg(_(e_cannot_go_beyond_last_matching_tag));
		    skip_msg = TRUE;
		}
		cur_match = num_matches - 1;
	    }
	    if (use_tagstack)
	    {
		tagptrs_T   tagp;

		tagstack[tagstackidx].cur_match = cur_match;
		tagstack[tagstackidx].cur_fnum = cur_fnum;

		// store user-provided data originating from tagfunc
		if (use_tfu && parse_match(matches[cur_match], &tagp) == OK
			&& tagp.user_data)
		{
		    VIM_CLEAR(tagstack[tagstackidx].user_data);
		    tagstack[tagstackidx].user_data = vim_strnsave(
			  tagp.user_data, tagp.user_data_end - tagp.user_data);
		}

		++tagstackidx;
	    }
#if defined(FEAT_QUICKFIX)
	    else if (g_do_tagpreview != 0)
	    {
		ptag_entry.cur_match = cur_match;
		ptag_entry.cur_fnum = cur_fnum;
	    }
#endif

	    /*
	     * Only when going to try the next match, report that the previous
	     * file didn't exist.  Otherwise an emsg() is given below.
	     */
	    if (nofile_fname != NULL && error_cur_match != cur_match)
		smsg(_("File \"%s\" does not exist"), nofile_fname);


	    ic = (matches[cur_match][0] & MT_IC_OFF);
	    if (type != DT_TAG && type != DT_SELECT && type != DT_JUMP
#ifdef FEAT_CSCOPE
		&& type != DT_CSCOPE
#endif
		&& (num_matches > 1 || ic)
		&& !skip_msg)
	    {
		// Give an indication of the number of matching tags
		sprintf((char *)IObuff, _("tag %d of %d%s"),
				cur_match + 1,
				num_matches,
				max_num_matches != MAXCOL ? _(" or more") : "");
		if (ic)
		    STRCAT(IObuff, _("  Using tag with different case!"));
		if ((num_matches > prev_num_matches || new_tag)
							   && num_matches > 1)
		{
		    if (ic)
			msg_attr((char *)IObuff, HL_ATTR(HLF_W));
		    else
			msg((char *)IObuff);
		    msg_scroll = TRUE;	// don't overwrite this message
		}
		else
		    give_warning(IObuff, ic);
		if (ic && !msg_scrolled && msg_silent == 0)
		{
		    out_flush();
		    ui_delay(1007L, TRUE);
		}
	    }

#if defined(FEAT_EVAL)
	    // Let the SwapExists event know what tag we are jumping to.
	    vim_snprintf((char *)IObuff, IOSIZE, ":ta %s\r", name);
	    set_vim_var_string(VV_SWAPCOMMAND, IObuff, -1);
#endif

	    /*
	     * Jump to the desired match.
	     */
	    i = jumpto_tag(matches[cur_match], forceit, type != DT_CSCOPE);

#if defined(FEAT_EVAL)
	    set_vim_var_string(VV_SWAPCOMMAND, NULL, -1);
#endif

	    if (i == NOTAGFILE)
	    {
		// File not found: try again with another matching tag
		if ((type == DT_PREV && cur_match > 0)
			|| ((type == DT_TAG || type == DT_NEXT
							  || type == DT_FIRST)
			    && (max_num_matches != MAXCOL
					     || cur_match < num_matches - 1)))
		{
		    error_cur_match = cur_match;
		    if (use_tagstack)
			--tagstackidx;
		    if (type == DT_PREV)
			--cur_match;
		    else
		    {
			type = DT_NEXT;
			++cur_match;
		    }
		    continue;
		}
		semsg(_(e_file_str_does_not_exist), nofile_fname);
	    }
	    else
	    {
		// We may have jumped to another window, check that
		// tagstackidx is still valid.
		if (use_tagstack && tagstackidx > curwin->w_tagstacklen)
		    tagstackidx = curwin->w_tagstackidx;
#ifdef FEAT_CSCOPE
		jumped_to_tag = TRUE;
#endif
	    }
	}
	break;
    }

end_do_tag:
    // Only store the new index when using the tagstack and it's valid.
    if (use_tagstack && tagstackidx <= curwin->w_tagstacklen)
	curwin->w_tagstackidx = tagstackidx;
    postponed_split = 0;	// don't split next time
# ifdef FEAT_QUICKFIX
    g_do_tagpreview = 0;	// don't do tag preview next time
# endif

    vim_free(tofree);
#ifdef FEAT_CSCOPE
    return jumped_to_tag;
#else
    return FALSE;
#endif
}