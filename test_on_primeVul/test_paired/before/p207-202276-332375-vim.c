block_insert(
    oparg_T		*oap,
    char_u		*s,
    int			b_insert,
    struct block_def	*bdp)
{
    int		ts_val;
    int		count = 0;	// extra spaces to replace a cut TAB
    int		spaces = 0;	// non-zero if cutting a TAB
    colnr_T	offset;		// pointer along new line
    colnr_T	startcol;	// column where insert starts
    unsigned	s_len;		// STRLEN(s)
    char_u	*newp, *oldp;	// new, old lines
    linenr_T	lnum;		// loop var
    int		oldstate = State;

    State = INSERT;		// don't want REPLACE for State
    s_len = (unsigned)STRLEN(s);

    for (lnum = oap->start.lnum + 1; lnum <= oap->end.lnum; lnum++)
    {
	block_prep(oap, bdp, lnum, TRUE);
	if (bdp->is_short && b_insert)
	    continue;	// OP_INSERT, line ends before block start

	oldp = ml_get(lnum);

	if (b_insert)
	{
	    ts_val = bdp->start_char_vcols;
	    spaces = bdp->startspaces;
	    if (spaces != 0)
		count = ts_val - 1; // we're cutting a TAB
	    offset = bdp->textcol;
	}
	else // append
	{
	    ts_val = bdp->end_char_vcols;
	    if (!bdp->is_short) // spaces = padding after block
	    {
		spaces = (bdp->endspaces ? ts_val - bdp->endspaces : 0);
		if (spaces != 0)
		    count = ts_val - 1; // we're cutting a TAB
		offset = bdp->textcol + bdp->textlen - (spaces != 0);
	    }
	    else // spaces = padding to block edge
	    {
		// if $ used, just append to EOL (ie spaces==0)
		if (!bdp->is_MAX)
		    spaces = (oap->end_vcol - bdp->end_vcol) + 1;
		count = spaces;
		offset = bdp->textcol + bdp->textlen;
	    }
	}

	if (has_mbyte && spaces > 0)
	{
	    int off;

	    // Avoid starting halfway a multi-byte character.
	    if (b_insert)
	    {
		off = (*mb_head_off)(oldp, oldp + offset + spaces);
		spaces -= off;
		count -= off;
	    }
	    else
	    {
		// spaces fill the gap, the character that's at the edge moves
		// right
		off = (*mb_head_off)(oldp, oldp + offset);
		offset -= off;
	    }
	}
	if (spaces < 0)  // can happen when the cursor was moved
	    spaces = 0;

	// Make sure the allocated size matches what is actually copied below.
	newp = alloc(STRLEN(oldp) + spaces + s_len
		    + (spaces > 0 && !bdp->is_short ? ts_val - spaces : 0)
								  + count + 1);
	if (newp == NULL)
	    continue;

	// copy up to shifted part
	mch_memmove(newp, oldp, (size_t)offset);
	oldp += offset;

	// insert pre-padding
	vim_memset(newp + offset, ' ', (size_t)spaces);
	startcol = offset + spaces;

	// copy the new text
	mch_memmove(newp + startcol, s, (size_t)s_len);
	offset += s_len;

	if (spaces > 0 && !bdp->is_short)
	{
	    if (*oldp == TAB)
	    {
		// insert post-padding
		vim_memset(newp + offset + spaces, ' ',
						    (size_t)(ts_val - spaces));
		// we're splitting a TAB, don't copy it
		oldp++;
		// We allowed for that TAB, remember this now
		count++;
	    }
	    else
		// Not a TAB, no extra spaces
		count = spaces;
	}

	if (spaces > 0)
	    offset += count;
	STRMOVE(newp + offset, oldp);

	ml_replace(lnum, newp, FALSE);

	if (b_insert)
	    // correct any text properties
	    inserted_bytes(lnum, startcol, s_len);

	if (lnum == oap->end.lnum)
	{
	    // Set "']" mark to the end of the block instead of the end of
	    // the insert in the first line.
	    curbuf->b_op_end.lnum = oap->end.lnum;
	    curbuf->b_op_end.col = offset;
	}
    } // for all lnum

    changed_lines(oap->start.lnum + 1, 0, oap->end.lnum + 1, 0L);

    State = oldstate;
}