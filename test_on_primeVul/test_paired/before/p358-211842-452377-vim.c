change_indent(
    int		type,
    int		amount,
    int		round,
    int		replaced,	// replaced character, put on replace stack
    int		call_changed_bytes)	// call changed_bytes()
{
    int		vcol;
    int		last_vcol;
    int		insstart_less;		// reduction for Insstart.col
    int		new_cursor_col;
    int		i;
    char_u	*ptr;
    int		save_p_list;
    int		start_col;
    colnr_T	vc;
    colnr_T	orig_col = 0;		// init for GCC
    char_u	*new_line, *orig_line = NULL;	// init for GCC

    // VREPLACE mode needs to know what the line was like before changing
    if (State & VREPLACE_FLAG)
    {
	orig_line = vim_strsave(ml_get_curline());  // Deal with NULL below
	orig_col = curwin->w_cursor.col;
    }

    // for the following tricks we don't want list mode
    save_p_list = curwin->w_p_list;
    curwin->w_p_list = FALSE;
    vc = getvcol_nolist(&curwin->w_cursor);
    vcol = vc;

    // For Replace mode we need to fix the replace stack later, which is only
    // possible when the cursor is in the indent.  Remember the number of
    // characters before the cursor if it's possible.
    start_col = curwin->w_cursor.col;

    // determine offset from first non-blank
    new_cursor_col = curwin->w_cursor.col;
    beginline(BL_WHITE);
    new_cursor_col -= curwin->w_cursor.col;

    insstart_less = curwin->w_cursor.col;

    // If the cursor is in the indent, compute how many screen columns the
    // cursor is to the left of the first non-blank.
    if (new_cursor_col < 0)
	vcol = get_indent() - vcol;

    if (new_cursor_col > 0)	    // can't fix replace stack
	start_col = -1;

    // Set the new indent.  The cursor will be put on the first non-blank.
    if (type == INDENT_SET)
	(void)set_indent(amount, call_changed_bytes ? SIN_CHANGED : 0);
    else
    {
	int	save_State = State;

	// Avoid being called recursively.
	if (State & VREPLACE_FLAG)
	    State = INSERT;
	shift_line(type == INDENT_DEC, round, 1, call_changed_bytes);
	State = save_State;
    }
    insstart_less -= curwin->w_cursor.col;

    // Try to put cursor on same character.
    // If the cursor is at or after the first non-blank in the line,
    // compute the cursor column relative to the column of the first
    // non-blank character.
    // If we are not in insert mode, leave the cursor on the first non-blank.
    // If the cursor is before the first non-blank, position it relative
    // to the first non-blank, counted in screen columns.
    if (new_cursor_col >= 0)
    {
	// When changing the indent while the cursor is touching it, reset
	// Insstart_col to 0.
	if (new_cursor_col == 0)
	    insstart_less = MAXCOL;
	new_cursor_col += curwin->w_cursor.col;
    }
    else if (!(State & INSERT))
	new_cursor_col = curwin->w_cursor.col;
    else
    {
	// Compute the screen column where the cursor should be.
	vcol = get_indent() - vcol;
	curwin->w_virtcol = (colnr_T)((vcol < 0) ? 0 : vcol);

	// Advance the cursor until we reach the right screen column.
	vcol = last_vcol = 0;
	new_cursor_col = -1;
	ptr = ml_get_curline();
	while (vcol <= (int)curwin->w_virtcol)
	{
	    last_vcol = vcol;
	    if (has_mbyte && new_cursor_col >= 0)
		new_cursor_col += (*mb_ptr2len)(ptr + new_cursor_col);
	    else
		++new_cursor_col;
	    vcol += lbr_chartabsize(ptr, ptr + new_cursor_col, (colnr_T)vcol);
	}
	vcol = last_vcol;

	// May need to insert spaces to be able to position the cursor on
	// the right screen column.
	if (vcol != (int)curwin->w_virtcol)
	{
	    curwin->w_cursor.col = (colnr_T)new_cursor_col;
	    i = (int)curwin->w_virtcol - vcol;
	    ptr = alloc(i + 1);
	    if (ptr != NULL)
	    {
		new_cursor_col += i;
		ptr[i] = NUL;
		while (--i >= 0)
		    ptr[i] = ' ';
		ins_str(ptr);
		vim_free(ptr);
	    }
	}

	// When changing the indent while the cursor is in it, reset
	// Insstart_col to 0.
	insstart_less = MAXCOL;
    }

    curwin->w_p_list = save_p_list;

    if (new_cursor_col <= 0)
	curwin->w_cursor.col = 0;
    else
	curwin->w_cursor.col = (colnr_T)new_cursor_col;
    curwin->w_set_curswant = TRUE;
    changed_cline_bef_curs();

    // May have to adjust the start of the insert.
    if (State & INSERT)
    {
	if (curwin->w_cursor.lnum == Insstart.lnum && Insstart.col != 0)
	{
	    if ((int)Insstart.col <= insstart_less)
		Insstart.col = 0;
	    else
		Insstart.col -= insstart_less;
	}
	if ((int)ai_col <= insstart_less)
	    ai_col = 0;
	else
	    ai_col -= insstart_less;
    }

    // For REPLACE mode, may have to fix the replace stack, if it's possible.
    // If the number of characters before the cursor decreased, need to pop a
    // few characters from the replace stack.
    // If the number of characters before the cursor increased, need to push a
    // few NULs onto the replace stack.
    if (REPLACE_NORMAL(State) && start_col >= 0)
    {
	while (start_col > (int)curwin->w_cursor.col)
	{
	    replace_join(0);	    // remove a NUL from the replace stack
	    --start_col;
	}
	while (start_col < (int)curwin->w_cursor.col || replaced)
	{
	    replace_push(NUL);
	    if (replaced)
	    {
		replace_push(replaced);
		replaced = NUL;
	    }
	    ++start_col;
	}
    }

    // For VREPLACE mode, we also have to fix the replace stack.  In this case
    // it is always possible because we backspace over the whole line and then
    // put it back again the way we wanted it.
    if (State & VREPLACE_FLAG)
    {
	// If orig_line didn't allocate, just return.  At least we did the job,
	// even if you can't backspace.
	if (orig_line == NULL)
	    return;

	// Save new line
	new_line = vim_strsave(ml_get_curline());
	if (new_line == NULL)
	    return;

	// We only put back the new line up to the cursor
	new_line[curwin->w_cursor.col] = NUL;

	// Put back original line
	ml_replace(curwin->w_cursor.lnum, orig_line, FALSE);
	curwin->w_cursor.col = orig_col;

	// Backspace from cursor to start of line
	backspace_until_column(0);

	// Insert new stuff into line again
	ins_bytes(new_line);

	vim_free(new_line);
    }
}