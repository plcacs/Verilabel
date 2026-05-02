win_redr_status(win_T *wp, int ignore_pum UNUSED)
{
    int		row;
    char_u	*p;
    int		len;
    int		fillchar;
    int		attr;
    int		this_ru_col;
    static int  busy = FALSE;

    // It's possible to get here recursively when 'statusline' (indirectly)
    // invokes ":redrawstatus".  Simply ignore the call then.
    if (busy)
	return;
    busy = TRUE;

    row = statusline_row(wp);

    wp->w_redr_status = FALSE;
    if (wp->w_status_height == 0)
    {
	// no status line, can only be last window
	redraw_cmdline = TRUE;
    }
    else if (!redrawing()
	    // don't update status line when popup menu is visible and may be
	    // drawn over it, unless it will be redrawn later
	    || (!ignore_pum && pum_visible()))
    {
	// Don't redraw right now, do it later.
	wp->w_redr_status = TRUE;
    }
#ifdef FEAT_STL_OPT
    else if (*p_stl != NUL || *wp->w_p_stl != NUL)
    {
	// redraw custom status line
	redraw_custom_statusline(wp);
    }
#endif
    else
    {
	fillchar = fillchar_status(&attr, wp);

	get_trans_bufname(wp->w_buffer);
	p = NameBuff;
	len = (int)STRLEN(p);

	if (bt_help(wp->w_buffer)
#ifdef FEAT_QUICKFIX
		|| wp->w_p_pvw
#endif
		|| bufIsChanged(wp->w_buffer)
		|| wp->w_buffer->b_p_ro)
	    *(p + len++) = ' ';
	if (bt_help(wp->w_buffer))
	{
	    vim_snprintf((char *)p + len, MAXPATHL - len, "%s", _("[Help]"));
	    len += (int)STRLEN(p + len);
	}
#ifdef FEAT_QUICKFIX
	if (wp->w_p_pvw)
	{
	    vim_snprintf((char *)p + len, MAXPATHL - len, "%s", _("[Preview]"));
	    len += (int)STRLEN(p + len);
	}
#endif
	if (bufIsChanged(wp->w_buffer)
#ifdef FEAT_TERMINAL
		&& !bt_terminal(wp->w_buffer)
#endif
		)
	{
	    vim_snprintf((char *)p + len, MAXPATHL - len, "%s", "[+]");
	    len += (int)STRLEN(p + len);
	}
	if (wp->w_buffer->b_p_ro)
	{
	    vim_snprintf((char *)p + len, MAXPATHL - len, "%s", _("[RO]"));
	    len += (int)STRLEN(p + len);
	}

	this_ru_col = ru_col - (Columns - wp->w_width);
	if (this_ru_col < (wp->w_width + 1) / 2)
	    this_ru_col = (wp->w_width + 1) / 2;
	if (this_ru_col <= 1)
	{
	    p = (char_u *)"<";		// No room for file name!
	    len = 1;
	}
	else if (has_mbyte)
	{
	    int	clen = 0, i;

	    // Count total number of display cells.
	    clen = mb_string2cells(p, -1);

	    // Find first character that will fit.
	    // Going from start to end is much faster for DBCS.
	    for (i = 0; p[i] != NUL && clen >= this_ru_col - 1;
		    i += (*mb_ptr2len)(p + i))
		clen -= (*mb_ptr2cells)(p + i);
	    len = clen;
	    if (i > 0)
	    {
		p = p + i - 1;
		*p = '<';
		++len;
	    }

	}
	else if (len > this_ru_col - 1)
	{
	    p += len - (this_ru_col - 1);
	    *p = '<';
	    len = this_ru_col - 1;
	}

	screen_puts(p, row, wp->w_wincol, attr);
	screen_fill(row, row + 1, len + wp->w_wincol,
			this_ru_col + wp->w_wincol, fillchar, fillchar, attr);

	if (get_keymap_str(wp, (char_u *)"<%s>", NameBuff, MAXPATHL)
		&& (int)(this_ru_col - len) > (int)(STRLEN(NameBuff) + 1))
	    screen_puts(NameBuff, row, (int)(this_ru_col - STRLEN(NameBuff)
						   - 1 + wp->w_wincol), attr);

#ifdef FEAT_CMDL_INFO
	win_redr_ruler(wp, TRUE, ignore_pum);
#endif
    }

    /*
     * May need to draw the character below the vertical separator.
     */
    if (wp->w_vsep_width != 0 && wp->w_status_height != 0 && redrawing())
    {
	if (stl_connected(wp))
	    fillchar = fillchar_status(&attr, wp);
	else
	    fillchar = fillchar_vsep(&attr);
	screen_putchar(fillchar, row, W_ENDCOL(wp), attr);
    }
    busy = FALSE;
}