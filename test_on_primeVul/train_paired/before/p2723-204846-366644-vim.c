nv_replace(cmdarg_T *cap)
{
    char_u	*ptr;
    int		had_ctrl_v;
    long	n;

    if (checkclearop(cap->oap))
	return;
#ifdef FEAT_JOB_CHANNEL
    if (bt_prompt(curbuf) && !prompt_curpos_editable())
    {
	clearopbeep(cap->oap);
	return;
    }
#endif

    // get another character
    if (cap->nchar == Ctrl_V)
    {
	had_ctrl_v = Ctrl_V;
	cap->nchar = get_literal(FALSE);
	// Don't redo a multibyte character with CTRL-V.
	if (cap->nchar > DEL)
	    had_ctrl_v = NUL;
    }
    else
	had_ctrl_v = NUL;

    // Abort if the character is a special key.
    if (IS_SPECIAL(cap->nchar))
    {
	clearopbeep(cap->oap);
	return;
    }

    // Visual mode "r"
    if (VIsual_active)
    {
	if (got_int)
	    reset_VIsual();
	if (had_ctrl_v)
	{
	    // Use a special (negative) number to make a difference between a
	    // literal CR or NL and a line break.
	    if (cap->nchar == CAR)
		cap->nchar = REPLACE_CR_NCHAR;
	    else if (cap->nchar == NL)
		cap->nchar = REPLACE_NL_NCHAR;
	}
	nv_operator(cap);
	return;
    }

    // Break tabs, etc.
    if (virtual_active())
    {
	if (u_save_cursor() == FAIL)
	    return;
	if (gchar_cursor() == NUL)
	{
	    // Add extra space and put the cursor on the first one.
	    coladvance_force((colnr_T)(getviscol() + cap->count1));
	    curwin->w_cursor.col -= cap->count1;
	}
	else if (gchar_cursor() == TAB)
	    coladvance_force(getviscol());
    }

    // Abort if not enough characters to replace.
    ptr = ml_get_cursor();
    if (STRLEN(ptr) < (unsigned)cap->count1
	    || (has_mbyte && mb_charlen(ptr) < cap->count1))
    {
	clearopbeep(cap->oap);
	return;
    }

    /*
     * Replacing with a TAB is done by edit() when it is complicated because
     * 'expandtab' or 'smarttab' is set.  CTRL-V TAB inserts a literal TAB.
     * Other characters are done below to avoid problems with things like
     * CTRL-V 048 (for edit() this would be R CTRL-V 0 ESC).
     */
    if (had_ctrl_v != Ctrl_V && cap->nchar == '\t' && (curbuf->b_p_et || p_sta))
    {
	stuffnumReadbuff(cap->count1);
	stuffcharReadbuff('R');
	stuffcharReadbuff('\t');
	stuffcharReadbuff(ESC);
	return;
    }

    // save line for undo
    if (u_save_cursor() == FAIL)
	return;

    if (had_ctrl_v != Ctrl_V && (cap->nchar == '\r' || cap->nchar == '\n'))
    {
	/*
	 * Replace character(s) by a single newline.
	 * Strange vi behaviour: Only one newline is inserted.
	 * Delete the characters here.
	 * Insert the newline with an insert command, takes care of
	 * autoindent.	The insert command depends on being on the last
	 * character of a line or not.
	 */
	(void)del_chars(cap->count1, FALSE);	// delete the characters
	stuffcharReadbuff('\r');
	stuffcharReadbuff(ESC);

	// Give 'r' to edit(), to get the redo command right.
	invoke_edit(cap, TRUE, 'r', FALSE);
    }
    else
    {
	prep_redo(cap->oap->regname, cap->count1,
				       NUL, 'r', NUL, had_ctrl_v, cap->nchar);

	curbuf->b_op_start = curwin->w_cursor;
	if (has_mbyte)
	{
	    int		old_State = State;

	    if (cap->ncharC1 != 0)
		AppendCharToRedobuff(cap->ncharC1);
	    if (cap->ncharC2 != 0)
		AppendCharToRedobuff(cap->ncharC2);

	    // This is slow, but it handles replacing a single-byte with a
	    // multi-byte and the other way around.  Also handles adding
	    // composing characters for utf-8.
	    for (n = cap->count1; n > 0; --n)
	    {
		State = REPLACE;
		if (cap->nchar == Ctrl_E || cap->nchar == Ctrl_Y)
		{
		    int c = ins_copychar(curwin->w_cursor.lnum
					   + (cap->nchar == Ctrl_Y ? -1 : 1));
		    if (c != NUL)
			ins_char(c);
		    else
			// will be decremented further down
			++curwin->w_cursor.col;
		}
		else
		    ins_char(cap->nchar);
		State = old_State;
		if (cap->ncharC1 != 0)
		    ins_char(cap->ncharC1);
		if (cap->ncharC2 != 0)
		    ins_char(cap->ncharC2);
	    }
	}
	else
	{
	    /*
	     * Replace the characters within one line.
	     */
	    for (n = cap->count1; n > 0; --n)
	    {
		/*
		 * Get ptr again, because u_save and/or showmatch() will have
		 * released the line.  At the same time we let know that the
		 * line will be changed.
		 */
		ptr = ml_get_buf(curbuf, curwin->w_cursor.lnum, TRUE);
		if (cap->nchar == Ctrl_E || cap->nchar == Ctrl_Y)
		{
		  int c = ins_copychar(curwin->w_cursor.lnum
					   + (cap->nchar == Ctrl_Y ? -1 : 1));
		  if (c != NUL)
		    ptr[curwin->w_cursor.col] = c;
		}
		else
		    ptr[curwin->w_cursor.col] = cap->nchar;
		if (p_sm && msg_silent == 0)
		    showmatch(cap->nchar);
		++curwin->w_cursor.col;
	    }
#ifdef FEAT_NETBEANS_INTG
	    if (netbeans_active())
	    {
		colnr_T  start = (colnr_T)(curwin->w_cursor.col - cap->count1);

		netbeans_removed(curbuf, curwin->w_cursor.lnum, start,
							   (long)cap->count1);
		netbeans_inserted(curbuf, curwin->w_cursor.lnum, start,
					       &ptr[start], (int)cap->count1);
	    }
#endif

	    // mark the buffer as changed and prepare for displaying
	    changed_bytes(curwin->w_cursor.lnum,
			       (colnr_T)(curwin->w_cursor.col - cap->count1));
	}
	--curwin->w_cursor.col;	    // cursor on the last replaced char
	// if the character on the left of the current cursor is a multi-byte
	// character, move two characters left
	if (has_mbyte)
	    mb_adjust_cursor();
	curbuf->b_op_end = curwin->w_cursor;
	curwin->w_set_curswant = TRUE;
	set_last_insert(cap->nchar);
    }
}