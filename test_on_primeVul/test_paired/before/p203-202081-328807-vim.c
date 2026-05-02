do_put(
    int		regname,
    char_u	*expr_result,	// result for regname "=" when compiled
    int		dir,		// BACKWARD for 'P', FORWARD for 'p'
    long	count,
    int		flags)
{
    char_u	*ptr;
    char_u	*newp, *oldp;
    int		yanklen;
    int		totlen = 0;		// init for gcc
    linenr_T	lnum;
    colnr_T	col;
    long	i;			// index in y_array[]
    int		y_type;
    long	y_size;
    int		oldlen;
    long	y_width = 0;
    colnr_T	vcol;
    int		delcount;
    int		incr = 0;
    long	j;
    struct block_def bd;
    char_u	**y_array = NULL;
    yankreg_T	*y_current_used = NULL;
    long	nr_lines = 0;
    pos_T	new_cursor;
    int		indent;
    int		orig_indent = 0;	// init for gcc
    int		indent_diff = 0;	// init for gcc
    int		first_indent = TRUE;
    int		lendiff = 0;
    pos_T	old_pos;
    char_u	*insert_string = NULL;
    int		allocated = FALSE;
    long	cnt;
    pos_T	orig_start = curbuf->b_op_start;
    pos_T	orig_end = curbuf->b_op_end;
    unsigned int cur_ve_flags = get_ve_flags();

#ifdef FEAT_CLIPBOARD
    // Adjust register name for "unnamed" in 'clipboard'.
    adjust_clip_reg(&regname);
    (void)may_get_selection(regname);
#endif

    if (flags & PUT_FIXINDENT)
	orig_indent = get_indent();

    curbuf->b_op_start = curwin->w_cursor;	// default for '[ mark
    curbuf->b_op_end = curwin->w_cursor;	// default for '] mark

    // Using inserted text works differently, because the register includes
    // special characters (newlines, etc.).
    if (regname == '.')
    {
	if (VIsual_active)
	    stuffcharReadbuff(VIsual_mode);
	(void)stuff_inserted((dir == FORWARD ? (count == -1 ? 'o' : 'a') :
				    (count == -1 ? 'O' : 'i')), count, FALSE);
	// Putting the text is done later, so can't really move the cursor to
	// the next character.  Use "l" to simulate it.
	if ((flags & PUT_CURSEND) && gchar_cursor() != NUL)
	    stuffcharReadbuff('l');
	return;
    }

    // For special registers '%' (file name), '#' (alternate file name) and
    // ':' (last command line), etc. we have to create a fake yank register.
    // For compiled code "expr_result" holds the expression result.
    if (regname == '=' && expr_result != NULL)
	insert_string = expr_result;
    else if (get_spec_reg(regname, &insert_string, &allocated, TRUE)
		&& insert_string == NULL)
	return;

    // Autocommands may be executed when saving lines for undo.  This might
    // make "y_array" invalid, so we start undo now to avoid that.
    if (u_save(curwin->w_cursor.lnum, curwin->w_cursor.lnum + 1) == FAIL)
	goto end;

    if (insert_string != NULL)
    {
	y_type = MCHAR;
#ifdef FEAT_EVAL
	if (regname == '=')
	{
	    // For the = register we need to split the string at NL
	    // characters.
	    // Loop twice: count the number of lines and save them.
	    for (;;)
	    {
		y_size = 0;
		ptr = insert_string;
		while (ptr != NULL)
		{
		    if (y_array != NULL)
			y_array[y_size] = ptr;
		    ++y_size;
		    ptr = vim_strchr(ptr, '\n');
		    if (ptr != NULL)
		    {
			if (y_array != NULL)
			    *ptr = NUL;
			++ptr;
			// A trailing '\n' makes the register linewise.
			if (*ptr == NUL)
			{
			    y_type = MLINE;
			    break;
			}
		    }
		}
		if (y_array != NULL)
		    break;
		y_array = ALLOC_MULT(char_u *, y_size);
		if (y_array == NULL)
		    goto end;
	    }
	}
	else
#endif
	{
	    y_size = 1;		// use fake one-line yank register
	    y_array = &insert_string;
	}
    }
    else
    {
	get_yank_register(regname, FALSE);

	y_type = y_current->y_type;
	y_width = y_current->y_width;
	y_size = y_current->y_size;
	y_array = y_current->y_array;
	y_current_used = y_current;
    }

    if (y_type == MLINE)
    {
	if (flags & PUT_LINE_SPLIT)
	{
	    char_u *p;

	    // "p" or "P" in Visual mode: split the lines to put the text in
	    // between.
	    if (u_save_cursor() == FAIL)
		goto end;
	    p = ml_get_cursor();
	    if (dir == FORWARD && *p != NUL)
		MB_PTR_ADV(p);
	    ptr = vim_strsave(p);
	    if (ptr == NULL)
		goto end;
	    ml_append(curwin->w_cursor.lnum, ptr, (colnr_T)0, FALSE);
	    vim_free(ptr);

	    oldp = ml_get_curline();
	    p = oldp + curwin->w_cursor.col;
	    if (dir == FORWARD && *p != NUL)
		MB_PTR_ADV(p);
	    ptr = vim_strnsave(oldp, p - oldp);
	    if (ptr == NULL)
		goto end;
	    ml_replace(curwin->w_cursor.lnum, ptr, FALSE);
	    ++nr_lines;
	    dir = FORWARD;
	}
	if (flags & PUT_LINE_FORWARD)
	{
	    // Must be "p" for a Visual block, put lines below the block.
	    curwin->w_cursor = curbuf->b_visual.vi_end;
	    dir = FORWARD;
	}
	curbuf->b_op_start = curwin->w_cursor;	// default for '[ mark
	curbuf->b_op_end = curwin->w_cursor;	// default for '] mark
    }

    if (flags & PUT_LINE)	// :put command or "p" in Visual line mode.
	y_type = MLINE;

    if (y_size == 0 || y_array == NULL)
    {
	semsg(_(e_nothing_in_register_str),
		  regname == 0 ? (char_u *)"\"" : transchar(regname));
	goto end;
    }

    if (y_type == MBLOCK)
    {
	lnum = curwin->w_cursor.lnum + y_size + 1;
	if (lnum > curbuf->b_ml.ml_line_count)
	    lnum = curbuf->b_ml.ml_line_count + 1;
	if (u_save(curwin->w_cursor.lnum - 1, lnum) == FAIL)
	    goto end;
    }
    else if (y_type == MLINE)
    {
	lnum = curwin->w_cursor.lnum;
#ifdef FEAT_FOLDING
	// Correct line number for closed fold.  Don't move the cursor yet,
	// u_save() uses it.
	if (dir == BACKWARD)
	    (void)hasFolding(lnum, &lnum, NULL);
	else
	    (void)hasFolding(lnum, NULL, &lnum);
#endif
	if (dir == FORWARD)
	    ++lnum;
	// In an empty buffer the empty line is going to be replaced, include
	// it in the saved lines.
	if ((BUFEMPTY() ? u_save(0, 2) : u_save(lnum - 1, lnum)) == FAIL)
	    goto end;
#ifdef FEAT_FOLDING
	if (dir == FORWARD)
	    curwin->w_cursor.lnum = lnum - 1;
	else
	    curwin->w_cursor.lnum = lnum;
	curbuf->b_op_start = curwin->w_cursor;	// for mark_adjust()
#endif
    }
    else if (u_save_cursor() == FAIL)
	goto end;

    yanklen = (int)STRLEN(y_array[0]);

    if (cur_ve_flags == VE_ALL && y_type == MCHAR)
    {
	if (gchar_cursor() == TAB)
	{
	    int viscol = getviscol();
	    int ts = curbuf->b_p_ts;

	    // Don't need to insert spaces when "p" on the last position of a
	    // tab or "P" on the first position.
	    if (dir == FORWARD ?
#ifdef FEAT_VARTABS
		    tabstop_padding(viscol, ts, curbuf->b_p_vts_array) != 1
#else
		    ts - (viscol % ts) != 1
#endif
		    : curwin->w_cursor.coladd > 0)
		coladvance_force(viscol);
	    else
		curwin->w_cursor.coladd = 0;
	}
	else if (curwin->w_cursor.coladd > 0 || gchar_cursor() == NUL)
	    coladvance_force(getviscol() + (dir == FORWARD));
    }

    lnum = curwin->w_cursor.lnum;
    col = curwin->w_cursor.col;

    // Block mode
    if (y_type == MBLOCK)
    {
	int	c = gchar_cursor();
	colnr_T	endcol2 = 0;

	if (dir == FORWARD && c != NUL)
	{
	    if (cur_ve_flags == VE_ALL)
		getvcol(curwin, &curwin->w_cursor, &col, NULL, &endcol2);
	    else
		getvcol(curwin, &curwin->w_cursor, NULL, NULL, &col);

	    if (has_mbyte)
		// move to start of next multi-byte character
		curwin->w_cursor.col += (*mb_ptr2len)(ml_get_cursor());
	    else
	    if (c != TAB || cur_ve_flags != VE_ALL)
		++curwin->w_cursor.col;
	    ++col;
	}
	else
	    getvcol(curwin, &curwin->w_cursor, &col, NULL, &endcol2);

	col += curwin->w_cursor.coladd;
	if (cur_ve_flags == VE_ALL
		&& (curwin->w_cursor.coladd > 0
		    || endcol2 == curwin->w_cursor.col))
	{
	    if (dir == FORWARD && c == NUL)
		++col;
	    if (dir != FORWARD && c != NUL && curwin->w_cursor.coladd > 0)
		++curwin->w_cursor.col;
	    if (c == TAB)
	    {
		if (dir == BACKWARD && curwin->w_cursor.col)
		    curwin->w_cursor.col--;
		if (dir == FORWARD && col - 1 == endcol2)
		    curwin->w_cursor.col++;
	    }
	}
	curwin->w_cursor.coladd = 0;
	bd.textcol = 0;
	for (i = 0; i < y_size; ++i)
	{
	    int spaces = 0;
	    char shortline;

	    bd.startspaces = 0;
	    bd.endspaces = 0;
	    vcol = 0;
	    delcount = 0;

	    // add a new line
	    if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
	    {
		if (ml_append(curbuf->b_ml.ml_line_count, (char_u *)"",
						   (colnr_T)1, FALSE) == FAIL)
		    break;
		++nr_lines;
	    }
	    // get the old line and advance to the position to insert at
	    oldp = ml_get_curline();
	    oldlen = (int)STRLEN(oldp);
	    for (ptr = oldp; vcol < col && *ptr; )
	    {
		// Count a tab for what it's worth (if list mode not on)
		incr = lbr_chartabsize_adv(oldp, &ptr, vcol);
		vcol += incr;
	    }
	    bd.textcol = (colnr_T)(ptr - oldp);

	    shortline = (vcol < col) || (vcol == col && !*ptr) ;

	    if (vcol < col) // line too short, padd with spaces
		bd.startspaces = col - vcol;
	    else if (vcol > col)
	    {
		bd.endspaces = vcol - col;
		bd.startspaces = incr - bd.endspaces;
		--bd.textcol;
		delcount = 1;
		if (has_mbyte)
		    bd.textcol -= (*mb_head_off)(oldp, oldp + bd.textcol);
		if (oldp[bd.textcol] != TAB)
		{
		    // Only a Tab can be split into spaces.  Other
		    // characters will have to be moved to after the
		    // block, causing misalignment.
		    delcount = 0;
		    bd.endspaces = 0;
		}
	    }

	    yanklen = (int)STRLEN(y_array[i]);

	    if ((flags & PUT_BLOCK_INNER) == 0)
	    {
		// calculate number of spaces required to fill right side of
		// block
		spaces = y_width + 1;
		for (j = 0; j < yanklen; j++)
		    spaces -= lbr_chartabsize(NULL, &y_array[i][j], 0);
		if (spaces < 0)
		    spaces = 0;
	    }

	    // Insert the new text.
	    // First check for multiplication overflow.
	    if (yanklen + spaces != 0
		     && count > ((INT_MAX - (bd.startspaces + bd.endspaces))
							/ (yanklen + spaces)))
	    {
		emsg(_(e_resulting_text_too_long));
		break;
	    }

	    totlen = count * (yanklen + spaces) + bd.startspaces + bd.endspaces;
	    newp = alloc(totlen + oldlen + 1);
	    if (newp == NULL)
		break;

	    // copy part up to cursor to new line
	    ptr = newp;
	    mch_memmove(ptr, oldp, (size_t)bd.textcol);
	    ptr += bd.textcol;

	    // may insert some spaces before the new text
	    vim_memset(ptr, ' ', (size_t)bd.startspaces);
	    ptr += bd.startspaces;

	    // insert the new text
	    for (j = 0; j < count; ++j)
	    {
		mch_memmove(ptr, y_array[i], (size_t)yanklen);
		ptr += yanklen;

		// insert block's trailing spaces only if there's text behind
		if ((j < count - 1 || !shortline) && spaces)
		{
		    vim_memset(ptr, ' ', (size_t)spaces);
		    ptr += spaces;
		}
	    }

	    // may insert some spaces after the new text
	    vim_memset(ptr, ' ', (size_t)bd.endspaces);
	    ptr += bd.endspaces;

	    // move the text after the cursor to the end of the line.
	    mch_memmove(ptr, oldp + bd.textcol + delcount,
				(size_t)(oldlen - bd.textcol - delcount + 1));
	    ml_replace(curwin->w_cursor.lnum, newp, FALSE);

	    ++curwin->w_cursor.lnum;
	    if (i == 0)
		curwin->w_cursor.col += bd.startspaces;
	}

	changed_lines(lnum, 0, curwin->w_cursor.lnum, nr_lines);

	// Set '[ mark.
	curbuf->b_op_start = curwin->w_cursor;
	curbuf->b_op_start.lnum = lnum;

	// adjust '] mark
	curbuf->b_op_end.lnum = curwin->w_cursor.lnum - 1;
	curbuf->b_op_end.col = bd.textcol + totlen - 1;
	curbuf->b_op_end.coladd = 0;
	if (flags & PUT_CURSEND)
	{
	    colnr_T len;

	    curwin->w_cursor = curbuf->b_op_end;
	    curwin->w_cursor.col++;

	    // in Insert mode we might be after the NUL, correct for that
	    len = (colnr_T)STRLEN(ml_get_curline());
	    if (curwin->w_cursor.col > len)
		curwin->w_cursor.col = len;
	}
	else
	    curwin->w_cursor.lnum = lnum;
    }
    else
    {
	// Character or Line mode
	if (y_type == MCHAR)
	{
	    // if type is MCHAR, FORWARD is the same as BACKWARD on the next
	    // char
	    if (dir == FORWARD && gchar_cursor() != NUL)
	    {
		if (has_mbyte)
		{
		    int bytelen = (*mb_ptr2len)(ml_get_cursor());

		    // put it on the next of the multi-byte character.
		    col += bytelen;
		    if (yanklen)
		    {
			curwin->w_cursor.col += bytelen;
			curbuf->b_op_end.col += bytelen;
		    }
		}
		else
		{
		    ++col;
		    if (yanklen)
		    {
			++curwin->w_cursor.col;
			++curbuf->b_op_end.col;
		    }
		}
	    }
	    curbuf->b_op_start = curwin->w_cursor;
	}
	// Line mode: BACKWARD is the same as FORWARD on the previous line
	else if (dir == BACKWARD)
	    --lnum;
	new_cursor = curwin->w_cursor;

	// simple case: insert into one line at a time
	if (y_type == MCHAR && y_size == 1)
	{
	    linenr_T	end_lnum = 0; // init for gcc
	    linenr_T	start_lnum = lnum;
	    int		first_byte_off = 0;

	    if (VIsual_active)
	    {
		end_lnum = curbuf->b_visual.vi_end.lnum;
		if (end_lnum < curbuf->b_visual.vi_start.lnum)
		    end_lnum = curbuf->b_visual.vi_start.lnum;
		if (end_lnum > start_lnum)
		{
		    pos_T   pos;

		    // "col" is valid for the first line, in following lines
		    // the virtual column needs to be used.  Matters for
		    // multi-byte characters.
		    pos.lnum = lnum;
		    pos.col = col;
		    pos.coladd = 0;
		    getvcol(curwin, &pos, NULL, &vcol, NULL);
		}
	    }

	    if (count == 0 || yanklen == 0)
	    {
		if (VIsual_active)
		    lnum = end_lnum;
	    }
	    else if (count > INT_MAX / yanklen)
		// multiplication overflow
		emsg(_(e_resulting_text_too_long));
	    else
	    {
		totlen = count * yanklen;
		do {
		    oldp = ml_get(lnum);
		    oldlen = (int)STRLEN(oldp);
		    if (lnum > start_lnum)
		    {
			pos_T   pos;

			pos.lnum = lnum;
			if (getvpos(&pos, vcol) == OK)
			    col = pos.col;
			else
			    col = MAXCOL;
		    }
		    if (VIsual_active && col > oldlen)
		    {
			lnum++;
			continue;
		    }
		    newp = alloc(totlen + oldlen + 1);
		    if (newp == NULL)
			goto end;	// alloc() gave an error message
		    mch_memmove(newp, oldp, (size_t)col);
		    ptr = newp + col;
		    for (i = 0; i < count; ++i)
		    {
			mch_memmove(ptr, y_array[0], (size_t)yanklen);
			ptr += yanklen;
		    }
		    STRMOVE(ptr, oldp + col);
		    ml_replace(lnum, newp, FALSE);

		    // compute the byte offset for the last character
		    first_byte_off = mb_head_off(newp, ptr - 1);

		    // Place cursor on last putted char.
		    if (lnum == curwin->w_cursor.lnum)
		    {
			// make sure curwin->w_virtcol is updated
			changed_cline_bef_curs();
			curwin->w_cursor.col += (colnr_T)(totlen - 1);
		    }
		    if (VIsual_active)
			lnum++;
		} while (VIsual_active && lnum <= end_lnum);

		if (VIsual_active) // reset lnum to the last visual line
		    lnum--;
	    }

	    // put '] at the first byte of the last character
	    curbuf->b_op_end = curwin->w_cursor;
	    curbuf->b_op_end.col -= first_byte_off;

	    // For "CTRL-O p" in Insert mode, put cursor after last char
	    if (totlen && (restart_edit != 0 || (flags & PUT_CURSEND)))
		++curwin->w_cursor.col;
	    else
		curwin->w_cursor.col -= first_byte_off;
	    changed_bytes(lnum, col);
	}
	else
	{
	    linenr_T	new_lnum = new_cursor.lnum;
	    size_t	len;

	    // Insert at least one line.  When y_type is MCHAR, break the first
	    // line in two.
	    for (cnt = 1; cnt <= count; ++cnt)
	    {
		i = 0;
		if (y_type == MCHAR)
		{
		    // Split the current line in two at the insert position.
		    // First insert y_array[size - 1] in front of second line.
		    // Then append y_array[0] to first line.
		    lnum = new_cursor.lnum;
		    ptr = ml_get(lnum) + col;
		    totlen = (int)STRLEN(y_array[y_size - 1]);
		    newp = alloc(STRLEN(ptr) + totlen + 1);
		    if (newp == NULL)
			goto error;
		    STRCPY(newp, y_array[y_size - 1]);
		    STRCAT(newp, ptr);
		    // insert second line
		    ml_append(lnum, newp, (colnr_T)0, FALSE);
		    ++new_lnum;
		    vim_free(newp);

		    oldp = ml_get(lnum);
		    newp = alloc(col + yanklen + 1);
		    if (newp == NULL)
			goto error;
					    // copy first part of line
		    mch_memmove(newp, oldp, (size_t)col);
					    // append to first line
		    mch_memmove(newp + col, y_array[0], (size_t)(yanklen + 1));
		    ml_replace(lnum, newp, FALSE);

		    curwin->w_cursor.lnum = lnum;
		    i = 1;
		}

		for (; i < y_size; ++i)
		{
		    if (y_type != MCHAR || i < y_size - 1)
		    {
			if (ml_append(lnum, y_array[i], (colnr_T)0, FALSE)
								      == FAIL)
			    goto error;
			new_lnum++;
		    }
		    lnum++;
		    ++nr_lines;
		    if (flags & PUT_FIXINDENT)
		    {
			old_pos = curwin->w_cursor;
			curwin->w_cursor.lnum = lnum;
			ptr = ml_get(lnum);
			if (cnt == count && i == y_size - 1)
			    lendiff = (int)STRLEN(ptr);
			if (*ptr == '#' && preprocs_left())
			    indent = 0;     // Leave # lines at start
			else
			     if (*ptr == NUL)
			    indent = 0;     // Ignore empty lines
			else if (first_indent)
			{
			    indent_diff = orig_indent - get_indent();
			    indent = orig_indent;
			    first_indent = FALSE;
			}
			else if ((indent = get_indent() + indent_diff) < 0)
			    indent = 0;
			(void)set_indent(indent, 0);
			curwin->w_cursor = old_pos;
			// remember how many chars were removed
			if (cnt == count && i == y_size - 1)
			    lendiff -= (int)STRLEN(ml_get(lnum));
		    }
		}
		if (cnt == 1)
		    new_lnum = lnum;
	    }

error:
	    // Adjust marks.
	    if (y_type == MLINE)
	    {
		curbuf->b_op_start.col = 0;
		if (dir == FORWARD)
		    curbuf->b_op_start.lnum++;
	    }
	    // Skip mark_adjust when adding lines after the last one, there
	    // can't be marks there. But still needed in diff mode.
	    if (curbuf->b_op_start.lnum + (y_type == MCHAR) - 1 + nr_lines
						 < curbuf->b_ml.ml_line_count
#ifdef FEAT_DIFF
						 || curwin->w_p_diff
#endif
						 )
		mark_adjust(curbuf->b_op_start.lnum + (y_type == MCHAR),
					     (linenr_T)MAXLNUM, nr_lines, 0L);

	    // note changed text for displaying and folding
	    if (y_type == MCHAR)
		changed_lines(curwin->w_cursor.lnum, col,
					 curwin->w_cursor.lnum + 1, nr_lines);
	    else
		changed_lines(curbuf->b_op_start.lnum, 0,
					   curbuf->b_op_start.lnum, nr_lines);
	    if (y_current_used != NULL && (y_current_used != y_current
					     || y_current->y_array != y_array))
	    {
		// Something invoked through changed_lines() has changed the
		// yank buffer, e.g. a GUI clipboard callback.
		emsg(_(e_yank_register_changed_while_using_it));
		goto end;
	    }

	    // Put the '] mark on the first byte of the last inserted character.
	    // Correct the length for change in indent.
	    curbuf->b_op_end.lnum = new_lnum;
	    len = STRLEN(y_array[y_size - 1]);
	    col = (colnr_T)len - lendiff;
	    if (col > 1)
	    {
		curbuf->b_op_end.col = col - 1;
		if (len > 0)
		    curbuf->b_op_end.col -= mb_head_off(y_array[y_size - 1],
						y_array[y_size - 1] + len - 1);
	    }
	    else
		curbuf->b_op_end.col = 0;

	    if (flags & PUT_CURSLINE)
	    {
		// ":put": put cursor on last inserted line
		curwin->w_cursor.lnum = lnum;
		beginline(BL_WHITE | BL_FIX);
	    }
	    else if (flags & PUT_CURSEND)
	    {
		// put cursor after inserted text
		if (y_type == MLINE)
		{
		    if (lnum >= curbuf->b_ml.ml_line_count)
			curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
		    else
			curwin->w_cursor.lnum = lnum + 1;
		    curwin->w_cursor.col = 0;
		}
		else
		{
		    curwin->w_cursor.lnum = new_lnum;
		    curwin->w_cursor.col = col;
		    curbuf->b_op_end = curwin->w_cursor;
		    if (col > 1)
			curbuf->b_op_end.col = col - 1;
		}
	    }
	    else if (y_type == MLINE)
	    {
		// put cursor on first non-blank in first inserted line
		curwin->w_cursor.col = 0;
		if (dir == FORWARD)
		    ++curwin->w_cursor.lnum;
		beginline(BL_WHITE | BL_FIX);
	    }
	    else	// put cursor on first inserted character
		curwin->w_cursor = new_cursor;
	}
    }

    msgmore(nr_lines);
    curwin->w_set_curswant = TRUE;

end:
    if (cmdmod.cmod_flags & CMOD_LOCKMARKS)
    {
	curbuf->b_op_start = orig_start;
	curbuf->b_op_end = orig_end;
    }
    if (allocated)
	vim_free(insert_string);
    if (regname == '=')
	vim_free(y_array);

    VIsual_active = FALSE;

    // If the cursor is past the end of the line put it at the end.
    adjust_cursor_eol();
}