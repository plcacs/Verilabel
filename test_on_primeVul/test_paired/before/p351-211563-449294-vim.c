n_start_visual_mode(int c)
{
#ifdef FEAT_CONCEAL
    int cursor_line_was_concealed = curwin->w_p_cole > 0
						&& conceal_cursor_line(curwin);
#endif

    VIsual_mode = c;
    VIsual_active = TRUE;
    VIsual_reselect = TRUE;
    trigger_modechanged();

    // Corner case: the 0 position in a tab may change when going into
    // virtualedit.  Recalculate curwin->w_cursor to avoid bad highlighting.
    if (c == Ctrl_V && (get_ve_flags() & VE_BLOCK) && gchar_cursor() == TAB)
    {
	validate_virtcol();
	coladvance(curwin->w_virtcol);
    }
    VIsual = curwin->w_cursor;

#ifdef FEAT_FOLDING
    foldAdjustVisual();
#endif

    setmouse();
#ifdef FEAT_CONCEAL
    // Check if redraw is needed after changing the state.
    conceal_check_cursor_line(cursor_line_was_concealed);
#endif

    if (p_smd && msg_silent == 0)
	redraw_cmdline = TRUE;	// show visual mode later
#ifdef FEAT_CLIPBOARD
    // Make sure the clipboard gets updated.  Needed because start and
    // end may still be the same, and the selection needs to be owned
    clip_star.vmode = NUL;
#endif

    // Only need to redraw this line, unless still need to redraw an old
    // Visual area (when 'lazyredraw' is set).
    if (curwin->w_redr_type < INVERTED)
    {
	curwin->w_old_cursor_lnum = curwin->w_cursor.lnum;
	curwin->w_old_visual_lnum = curwin->w_cursor.lnum;
    }
}