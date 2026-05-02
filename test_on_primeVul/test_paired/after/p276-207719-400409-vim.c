display_dollar(colnr_T col_arg)
{
    colnr_T col = col_arg < 0 ? 0 : col_arg;
    colnr_T save_col;

    if (!redrawing())
	return;

    cursor_off();
    save_col = curwin->w_cursor.col;
    curwin->w_cursor.col = col;
    if (has_mbyte)
    {
	char_u *p;

	// If on the last byte of a multi-byte move to the first byte.
	p = ml_get_curline();
	curwin->w_cursor.col -= (*mb_head_off)(p, p + col);
    }
    curs_columns(FALSE);	    // recompute w_wrow and w_wcol
    if (curwin->w_wcol < curwin->w_width)
    {
	edit_putchar('$', FALSE);
	dollar_vcol = curwin->w_virtcol;
    }
    curwin->w_cursor.col = save_col;
}