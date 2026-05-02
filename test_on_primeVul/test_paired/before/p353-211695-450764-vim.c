reg_match_visual(void)
{
    pos_T	top, bot;
    linenr_T    lnum;
    colnr_T	col;
    win_T	*wp = rex.reg_win == NULL ? curwin : rex.reg_win;
    int		mode;
    colnr_T	start, end;
    colnr_T	start2, end2;
    colnr_T	cols;
    colnr_T	curswant;

    // Check if the buffer is the current buffer.
    if (rex.reg_buf != curbuf || VIsual.lnum == 0)
	return FALSE;

    if (VIsual_active)
    {
	if (LT_POS(VIsual, wp->w_cursor))
	{
	    top = VIsual;
	    bot = wp->w_cursor;
	}
	else
	{
	    top = wp->w_cursor;
	    bot = VIsual;
	}
	mode = VIsual_mode;
	curswant = wp->w_curswant;
    }
    else
    {
	if (LT_POS(curbuf->b_visual.vi_start, curbuf->b_visual.vi_end))
	{
	    top = curbuf->b_visual.vi_start;
	    bot = curbuf->b_visual.vi_end;
	}
	else
	{
	    top = curbuf->b_visual.vi_end;
	    bot = curbuf->b_visual.vi_start;
	}
	mode = curbuf->b_visual.vi_mode;
	curswant = curbuf->b_visual.vi_curswant;
    }
    lnum = rex.lnum + rex.reg_firstlnum;
    if (lnum < top.lnum || lnum > bot.lnum)
	return FALSE;

    if (mode == 'v')
    {
	col = (colnr_T)(rex.input - rex.line);
	if ((lnum == top.lnum && col < top.col)
		|| (lnum == bot.lnum && col >= bot.col + (*p_sel != 'e')))
	    return FALSE;
    }
    else if (mode == Ctrl_V)
    {
	getvvcol(wp, &top, &start, NULL, &end);
	getvvcol(wp, &bot, &start2, NULL, &end2);
	if (start2 < start)
	    start = start2;
	if (end2 > end)
	    end = end2;
	if (top.col == MAXCOL || bot.col == MAXCOL || curswant == MAXCOL)
	    end = MAXCOL;
	cols = win_linetabsize(wp, rex.line, (colnr_T)(rex.input - rex.line));
	if (cols < start || cols > end - (*p_sel == 'e'))
	    return FALSE;
    }
    return TRUE;
}