win_enter_ext(
    win_T	*wp,
    int		undo_sync,
    int		curwin_invalid,
    int		trigger_new_autocmds,
    int		trigger_enter_autocmds,
    int		trigger_leave_autocmds)
{
    int		other_buffer = FALSE;

    if (wp == curwin && !curwin_invalid)	/* nothing to do */
	return;

#ifdef FEAT_JOB_CHANNEL
    if (!curwin_invalid)
	leaving_window(curwin);
#endif

    if (!curwin_invalid && trigger_leave_autocmds)
    {
	/*
	 * Be careful: If autocommands delete the window, return now.
	 */
	if (wp->w_buffer != curbuf)
	{
	    apply_autocmds(EVENT_BUFLEAVE, NULL, NULL, FALSE, curbuf);
	    other_buffer = TRUE;
	    if (!win_valid(wp))
		return;
	}
	apply_autocmds(EVENT_WINLEAVE, NULL, NULL, FALSE, curbuf);
	if (!win_valid(wp))
	    return;
#ifdef FEAT_EVAL
	/* autocmds may abort script processing */
	if (aborting())
	    return;
#endif
    }

    /* sync undo before leaving the current buffer */
    if (undo_sync && curbuf != wp->w_buffer)
	u_sync(FALSE);

    /* Might need to scroll the old window before switching, e.g., when the
     * cursor was moved. */
    update_topline();

    /* may have to copy the buffer options when 'cpo' contains 'S' */
    if (wp->w_buffer != curbuf)
	buf_copy_options(wp->w_buffer, BCO_ENTER | BCO_NOHELP);
    if (!curwin_invalid)
    {
	prevwin = curwin;	/* remember for CTRL-W p */
	curwin->w_redr_status = TRUE;
    }
    curwin = wp;
    curbuf = wp->w_buffer;
    check_cursor();
    if (!virtual_active())
	curwin->w_cursor.coladd = 0;
    changed_line_abv_curs();	/* assume cursor position needs updating */

    if (curwin->w_localdir != NULL || curtab->tp_localdir != NULL)
    {
	char_u	*dirname;

	// Window or tab has a local directory: Save current directory as
	// global directory (unless that was done already) and change to the
	// local directory.
	if (globaldir == NULL)
	{
	    char_u	cwd[MAXPATHL];

	    if (mch_dirname(cwd, MAXPATHL) == OK)
		globaldir = vim_strsave(cwd);
	}
	if (curwin->w_localdir != NULL)
	    dirname = curwin->w_localdir;
	else
	    dirname = curtab->tp_localdir;

	if (mch_chdir((char *)dirname) == 0)
	    shorten_fnames(TRUE);
    }
    else if (globaldir != NULL)
    {
	/* Window doesn't have a local directory and we are not in the global
	 * directory: Change to the global directory. */
	vim_ignored = mch_chdir((char *)globaldir);
	VIM_CLEAR(globaldir);
	shorten_fnames(TRUE);
    }

#ifdef FEAT_JOB_CHANNEL
    entering_window(curwin);
#endif
    if (trigger_new_autocmds)
	apply_autocmds(EVENT_WINNEW, NULL, NULL, FALSE, curbuf);
    if (trigger_enter_autocmds)
    {
	apply_autocmds(EVENT_WINENTER, NULL, NULL, FALSE, curbuf);
	if (other_buffer)
	    apply_autocmds(EVENT_BUFENTER, NULL, NULL, FALSE, curbuf);
    }

#ifdef FEAT_TITLE
    maketitle();
#endif
    curwin->w_redr_status = TRUE;
#ifdef FEAT_TERMINAL
    if (bt_terminal(wp->w_buffer))
	// terminal is likely in another mode
	redraw_mode = TRUE;
#endif
    redraw_tabline = TRUE;
    if (restart_edit)
	redraw_later(VALID);	/* causes status line redraw */

    /* set window height to desired minimal value */
    if (curwin->w_height < p_wh && !curwin->w_p_wfh
#ifdef FEAT_TEXT_PROP
	    && !popup_is_popup(curwin)
#endif
	    )
	win_setheight((int)p_wh);
    else if (curwin->w_height == 0)
	win_setheight(1);

    /* set window width to desired minimal value */
    if (curwin->w_width < p_wiw && !curwin->w_p_wfw)
	win_setwidth((int)p_wiw);

    setmouse();			// in case jumped to/from help buffer

    /* Change directories when the 'acd' option is set. */
    DO_AUTOCHDIR;
}