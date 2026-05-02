do_arg_all(
    int	count,
    int	forceit,		// hide buffers in current windows
    int keep_tabs)		// keep current tabs, for ":tab drop file"
{
    int		i;
    win_T	*wp, *wpnext;
    char_u	*opened;	// Array of weight for which args are open:
				//  0: not opened
				//  1: opened in other tab
				//  2: opened in curtab
				//  3: opened in curtab and curwin
				//
    int		opened_len;	// length of opened[]
    int		use_firstwin = FALSE;	// use first window for arglist
    int		tab_drop_empty_window = FALSE;
    int		split_ret = OK;
    int		p_ea_save;
    alist_T	*alist;		// argument list to be used
    buf_T	*buf;
    tabpage_T	*tpnext;
    int		had_tab = cmdmod.cmod_tab;
    win_T	*old_curwin, *last_curwin;
    tabpage_T	*old_curtab, *last_curtab;
    win_T	*new_curwin = NULL;
    tabpage_T	*new_curtab = NULL;

#ifdef FEAT_CMDWIN
    if (cmdwin_type != 0)
    {
	emsg(_(e_invalid_in_cmdline_window));
	return;
    }
#endif
    if (ARGCOUNT <= 0)
    {
	// Don't give an error message.  We don't want it when the ":all"
	// command is in the .vimrc.
	return;
    }
    setpcmark();

    opened_len = ARGCOUNT;
    opened = alloc_clear(opened_len);
    if (opened == NULL)
	return;

    // Autocommands may do anything to the argument list.  Make sure it's not
    // freed while we are working here by "locking" it.  We still have to
    // watch out for its size to be changed.
    alist = curwin->w_alist;
    ++alist->al_refcount;

    old_curwin = curwin;
    old_curtab = curtab;

# ifdef FEAT_GUI
    need_mouse_correct = TRUE;
# endif

    // Try closing all windows that are not in the argument list.
    // Also close windows that are not full width;
    // When 'hidden' or "forceit" set the buffer becomes hidden.
    // Windows that have a changed buffer and can't be hidden won't be closed.
    // When the ":tab" modifier was used do this for all tab pages.
    if (had_tab > 0)
	goto_tabpage_tp(first_tabpage, TRUE, TRUE);
    for (;;)
    {
	tpnext = curtab->tp_next;
	for (wp = firstwin; wp != NULL; wp = wpnext)
	{
	    wpnext = wp->w_next;
	    buf = wp->w_buffer;
	    if (buf->b_ffname == NULL
		    || (!keep_tabs && (buf->b_nwindows > 1
			    || wp->w_width != Columns)))
		i = opened_len;
	    else
	    {
		// check if the buffer in this window is in the arglist
		for (i = 0; i < opened_len; ++i)
		{
		    if (i < alist->al_ga.ga_len
			    && (AARGLIST(alist)[i].ae_fnum == buf->b_fnum
				|| fullpathcmp(alist_name(&AARGLIST(alist)[i]),
					buf->b_ffname, TRUE, TRUE) & FPC_SAME))
		    {
			int weight = 1;

			if (old_curtab == curtab)
			{
			    ++weight;
			    if (old_curwin == wp)
				++weight;
			}

			if (weight > (int)opened[i])
			{
			    opened[i] = (char_u)weight;
			    if (i == 0)
			    {
				if (new_curwin != NULL)
				    new_curwin->w_arg_idx = opened_len;
				new_curwin = wp;
				new_curtab = curtab;
			    }
			}
			else if (keep_tabs)
			    i = opened_len;

			if (wp->w_alist != alist)
			{
			    // Use the current argument list for all windows
			    // containing a file from it.
			    alist_unlink(wp->w_alist);
			    wp->w_alist = alist;
			    ++wp->w_alist->al_refcount;
			}
			break;
		    }
		}
	    }
	    wp->w_arg_idx = i;

	    if (i == opened_len && !keep_tabs)// close this window
	    {
		if (buf_hide(buf) || forceit || buf->b_nwindows > 1
							|| !bufIsChanged(buf))
		{
		    // If the buffer was changed, and we would like to hide it,
		    // try autowriting.
		    if (!buf_hide(buf) && buf->b_nwindows <= 1
							 && bufIsChanged(buf))
		    {
			bufref_T    bufref;

			set_bufref(&bufref, buf);

			(void)autowrite(buf, FALSE);

			// check if autocommands removed the window
			if (!win_valid(wp) || !bufref_valid(&bufref))
			{
			    wpnext = firstwin;	// start all over...
			    continue;
			}
		    }
		    // don't close last window
		    if (ONE_WINDOW
			    && (first_tabpage->tp_next == NULL || !had_tab))
			use_firstwin = TRUE;
		    else
		    {
			win_close(wp, !buf_hide(buf) && !bufIsChanged(buf));

			// check if autocommands removed the next window
			if (!win_valid(wpnext))
			    wpnext = firstwin;	// start all over...
		    }
		}
	    }
	}

	// Without the ":tab" modifier only do the current tab page.
	if (had_tab == 0 || tpnext == NULL)
	    break;

	// check if autocommands removed the next tab page
	if (!valid_tabpage(tpnext))
	    tpnext = first_tabpage;	// start all over...

	goto_tabpage_tp(tpnext, TRUE, TRUE);
    }

    // Open a window for files in the argument list that don't have one.
    // ARGCOUNT may change while doing this, because of autocommands.
    if (count > opened_len || count <= 0)
	count = opened_len;

    // Don't execute Win/Buf Enter/Leave autocommands here.
    ++autocmd_no_enter;
    ++autocmd_no_leave;
    last_curwin = curwin;
    last_curtab = curtab;
    win_enter(lastwin, FALSE);
    // ":tab drop file" should re-use an empty window to avoid "--remote-tab"
    // leaving an empty tab page when executed locally.
    if (keep_tabs && BUFEMPTY() && curbuf->b_nwindows == 1
			    && curbuf->b_ffname == NULL && !curbuf->b_changed)
    {
	use_firstwin = TRUE;
	tab_drop_empty_window = TRUE;
    }

    for (i = 0; i < count && !got_int; ++i)
    {
	if (alist == &global_alist && i == global_alist.al_ga.ga_len - 1)
	    arg_had_last = TRUE;
	if (opened[i] > 0)
	{
	    // Move the already present window to below the current window
	    if (curwin->w_arg_idx != i)
	    {
		FOR_ALL_WINDOWS(wpnext)
		{
		    if (wpnext->w_arg_idx == i)
		    {
			if (keep_tabs)
			{
			    new_curwin = wpnext;
			    new_curtab = curtab;
			}
			else if (wpnext->w_frame->fr_parent
						 != curwin->w_frame->fr_parent)
			{
			    emsg(_("E249: window layout changed unexpectedly"));
			    i = count;
			    break;
			}
			else
			    win_move_after(wpnext, curwin);
			break;
		    }
		}
	    }
	}
	else if (split_ret == OK)
	{
	    // trigger events for tab drop
	    if (tab_drop_empty_window && i == count - 1)
		--autocmd_no_enter;
	    if (!use_firstwin)		// split current window
	    {
		p_ea_save = p_ea;
		p_ea = TRUE;		// use space from all windows
		split_ret = win_split(0, WSP_ROOM | WSP_BELOW);
		p_ea = p_ea_save;
		if (split_ret == FAIL)
		    continue;
	    }
	    else    // first window: do autocmd for leaving this buffer
		--autocmd_no_leave;

	    // edit file "i"
	    curwin->w_arg_idx = i;
	    if (i == 0)
	    {
		new_curwin = curwin;
		new_curtab = curtab;
	    }
	    (void)do_ecmd(0, alist_name(&AARGLIST(alist)[i]), NULL, NULL,
		      ECMD_ONE,
		      ((buf_hide(curwin->w_buffer)
			   || bufIsChanged(curwin->w_buffer)) ? ECMD_HIDE : 0)
						       + ECMD_OLDBUF, curwin);
	    if (tab_drop_empty_window && i == count - 1)
		++autocmd_no_enter;
	    if (use_firstwin)
		++autocmd_no_leave;
	    use_firstwin = FALSE;
	}
	ui_breakcheck();

	// When ":tab" was used open a new tab for a new window repeatedly.
	if (had_tab > 0 && tabpage_index(NULL) <= p_tpm)
	    cmdmod.cmod_tab = 9999;
    }

    // Remove the "lock" on the argument list.
    alist_unlink(alist);

    --autocmd_no_enter;

    // restore last referenced tabpage's curwin
    if (last_curtab != new_curtab)
    {
	if (valid_tabpage(last_curtab))
	    goto_tabpage_tp(last_curtab, TRUE, TRUE);
	if (win_valid(last_curwin))
	    win_enter(last_curwin, FALSE);
    }
    // to window with first arg
    if (valid_tabpage(new_curtab))
	goto_tabpage_tp(new_curtab, TRUE, TRUE);
    if (win_valid(new_curwin))
	win_enter(new_curwin, FALSE);

    --autocmd_no_leave;
    vim_free(opened);
}