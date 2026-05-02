cmdline_insert_reg(int *gotesc UNUSED)
{
    int		i;
    int		c;
    int		save_new_cmdpos = new_cmdpos;

#ifdef USE_ON_FLY_SCROLL
    dont_scroll = TRUE;	// disallow scrolling here
#endif
    putcmdline('"', TRUE);
    ++no_mapping;
    ++allow_keys;
    i = c = plain_vgetc();	// CTRL-R <char>
    if (i == Ctrl_O)
	i = Ctrl_R;		// CTRL-R CTRL-O == CTRL-R CTRL-R
    if (i == Ctrl_R)
	c = plain_vgetc();	// CTRL-R CTRL-R <char>
    extra_char = NUL;
    --no_mapping;
    --allow_keys;
#ifdef FEAT_EVAL
    /*
     * Insert the result of an expression.
     */
    new_cmdpos = -1;
    if (c == '=')
    {
	if (ccline.cmdfirstc == '='  // can't do this recursively
		|| cmdline_star > 0) // or when typing a password
	{
	    beep_flush();
	    c = ESC;
	}
	else
	    c = get_expr_register();
    }
#endif
    if (c != ESC)	    // use ESC to cancel inserting register
    {
	cmdline_paste(c, i == Ctrl_R, FALSE);

#ifdef FEAT_EVAL
	// When there was a serious error abort getting the
	// command line.
	if (aborting())
	{
	    *gotesc = TRUE;  // will free ccline.cmdbuff after
	    // putting it in history
	    return GOTO_NORMAL_MODE;
	}
#endif
	KeyTyped = FALSE;	// Don't do p_wc completion.
#ifdef FEAT_EVAL
	if (new_cmdpos >= 0)
	{
	    // set_cmdline_pos() was used
	    if (new_cmdpos > ccline.cmdlen)
		ccline.cmdpos = ccline.cmdlen;
	    else
		ccline.cmdpos = new_cmdpos;
	}
#endif
    }
    new_cmdpos = save_new_cmdpos;

    // remove the double quote
    redrawcmd();

    // The text has been stuffed, the command line didn't change yet.
    return CMDLINE_NOT_CHANGED;
}