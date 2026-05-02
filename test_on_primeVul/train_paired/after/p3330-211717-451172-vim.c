did_set_string_option(
    int		opt_idx,		/* index in options[] table */
    char_u	**varp,			/* pointer to the option variable */
    int		new_value_alloced,	/* new value was allocated */
    char_u	*oldval,		/* previous value of the option */
    char_u	*errbuf,		/* buffer for errors, or NULL */
    int		opt_flags)		/* OPT_LOCAL and/or OPT_GLOBAL */
{
    char_u	*errmsg = NULL;
    char_u	*s, *p;
    int		did_chartab = FALSE;
    char_u	**gvarp;
    long_u	free_oldval = (options[opt_idx].flags & P_ALLOCED);
#ifdef FEAT_GUI
    /* set when changing an option that only requires a redraw in the GUI */
    int		redraw_gui_only = FALSE;
#endif

    /* Get the global option to compare with, otherwise we would have to check
     * two values for all local options. */
    gvarp = (char_u **)get_varp_scope(&(options[opt_idx]), OPT_GLOBAL);

    /* Disallow changing some options from secure mode */
    if ((secure
#ifdef HAVE_SANDBOX
		|| sandbox != 0
#endif
		) && (options[opt_idx].flags & P_SECURE))
    {
	errmsg = e_secure;
    }

    /* Check for a "normal" file name in some options.  Disallow a path
     * separator (slash and/or backslash), wildcards and characters that are
     * often illegal in a file name. */
    else if ((options[opt_idx].flags & P_NFNAME)
			 && vim_strpbrk(*varp, (char_u *)"/\\*?[|<>") != NULL)
    {
	errmsg = e_invarg;
    }

    /* 'term' */
    else if (varp == &T_NAME)
    {
	if (T_NAME[0] == NUL)
	    errmsg = (char_u *)N_("E529: Cannot set 'term' to empty string");
#ifdef FEAT_GUI
	if (gui.in_use)
	    errmsg = (char_u *)N_("E530: Cannot change term in GUI");
	else if (term_is_gui(T_NAME))
	    errmsg = (char_u *)N_("E531: Use \":gui\" to start the GUI");
#endif
	else if (set_termname(T_NAME) == FAIL)
	    errmsg = (char_u *)N_("E522: Not found in termcap");
	else
	    /* Screen colors may have changed. */
	    redraw_later_clear();
    }

    /* 'backupcopy' */
    else if (gvarp == &p_bkc)
    {
	char_u		*bkc = p_bkc;
	unsigned int	*flags = &bkc_flags;

	if (opt_flags & OPT_LOCAL)
	{
	    bkc = curbuf->b_p_bkc;
	    flags = &curbuf->b_bkc_flags;
	}

	if ((opt_flags & OPT_LOCAL) && *bkc == NUL)
	    /* make the local value empty: use the global value */
	    *flags = 0;
	else
	{
	    if (opt_strings_flags(bkc, p_bkc_values, flags, TRUE) != OK)
		errmsg = e_invarg;
	    if ((((int)*flags & BKC_AUTO) != 0)
		    + (((int)*flags & BKC_YES) != 0)
		    + (((int)*flags & BKC_NO) != 0) != 1)
	    {
		/* Must have exactly one of "auto", "yes"  and "no". */
		(void)opt_strings_flags(oldval, p_bkc_values, flags, TRUE);
		errmsg = e_invarg;
	    }
	}
    }

    /* 'backupext' and 'patchmode' */
    else if (varp == &p_bex || varp == &p_pm)
    {
	if (STRCMP(*p_bex == '.' ? p_bex + 1 : p_bex,
		     *p_pm == '.' ? p_pm + 1 : p_pm) == 0)
	    errmsg = (char_u *)N_("E589: 'backupext' and 'patchmode' are equal");
    }
#ifdef FEAT_LINEBREAK
    /* 'breakindentopt' */
    else if (varp == &curwin->w_p_briopt)
    {
	if (briopt_check(curwin) == FAIL)
	    errmsg = e_invarg;
    }
#endif

    /*
     * 'isident', 'iskeyword', 'isprint or 'isfname' option: refill g_chartab[]
     * If the new option is invalid, use old value.  'lisp' option: refill
     * g_chartab[] for '-' char
     */
    else if (  varp == &p_isi
	    || varp == &(curbuf->b_p_isk)
	    || varp == &p_isp
	    || varp == &p_isf)
    {
	if (init_chartab() == FAIL)
	{
	    did_chartab = TRUE;	    /* need to restore it below */
	    errmsg = e_invarg;	    /* error in value */
	}
    }

    /* 'helpfile' */
    else if (varp == &p_hf)
    {
	/* May compute new values for $VIM and $VIMRUNTIME */
	if (didset_vim)
	{
	    vim_setenv((char_u *)"VIM", (char_u *)"");
	    didset_vim = FALSE;
	}
	if (didset_vimruntime)
	{
	    vim_setenv((char_u *)"VIMRUNTIME", (char_u *)"");
	    didset_vimruntime = FALSE;
	}
    }

#ifdef FEAT_SYN_HL
    /* 'colorcolumn' */
    else if (varp == &curwin->w_p_cc)
	errmsg = check_colorcolumn(curwin);
#endif

#ifdef FEAT_MULTI_LANG
    /* 'helplang' */
    else if (varp == &p_hlg)
    {
	/* Check for "", "ab", "ab,cd", etc. */
	for (s = p_hlg; *s != NUL; s += 3)
	{
	    if (s[1] == NUL || ((s[2] != ',' || s[3] == NUL) && s[2] != NUL))
	    {
		errmsg = e_invarg;
		break;
	    }
	    if (s[2] == NUL)
		break;
	}
    }
#endif

    /* 'highlight' */
    else if (varp == &p_hl)
    {
	if (highlight_changed() == FAIL)
	    errmsg = e_invarg;	/* invalid flags */
    }

    /* 'nrformats' */
    else if (gvarp == &p_nf)
    {
	if (check_opt_strings(*varp, p_nf_values, TRUE) != OK)
	    errmsg = e_invarg;
    }

#ifdef FEAT_SESSION
    /* 'sessionoptions' */
    else if (varp == &p_ssop)
    {
	if (opt_strings_flags(p_ssop, p_ssop_values, &ssop_flags, TRUE) != OK)
	    errmsg = e_invarg;
	if ((ssop_flags & SSOP_CURDIR) && (ssop_flags & SSOP_SESDIR))
	{
	    /* Don't allow both "sesdir" and "curdir". */
	    (void)opt_strings_flags(oldval, p_ssop_values, &ssop_flags, TRUE);
	    errmsg = e_invarg;
	}
    }
    /* 'viewoptions' */
    else if (varp == &p_vop)
    {
	if (opt_strings_flags(p_vop, p_ssop_values, &vop_flags, TRUE) != OK)
	    errmsg = e_invarg;
    }
#endif

    /* 'scrollopt' */
#ifdef FEAT_SCROLLBIND
    else if (varp == &p_sbo)
    {
	if (check_opt_strings(p_sbo, p_scbopt_values, TRUE) != OK)
	    errmsg = e_invarg;
    }
#endif

    /* 'ambiwidth' */
#ifdef FEAT_MBYTE
    else if (varp == &p_ambw || varp == &p_emoji)
    {
	if (check_opt_strings(p_ambw, p_ambw_values, FALSE) != OK)
	    errmsg = e_invarg;
	else if (set_chars_option(&p_lcs) != NULL)
	    errmsg = (char_u *)_("E834: Conflicts with value of 'listchars'");
# if defined(FEAT_WINDOWS) || defined(FEAT_FOLDING)
	else if (set_chars_option(&p_fcs) != NULL)
	    errmsg = (char_u *)_("E835: Conflicts with value of 'fillchars'");
# endif
    }
#endif

    /* 'background' */
    else if (varp == &p_bg)
    {
	if (check_opt_strings(p_bg, p_bg_values, FALSE) == OK)
	{
#ifdef FEAT_EVAL
	    int dark = (*p_bg == 'd');
#endif

	    init_highlight(FALSE, FALSE);

#ifdef FEAT_EVAL
	    if (dark != (*p_bg == 'd')
			  && get_var_value((char_u *)"g:colors_name") != NULL)
	    {
		/* The color scheme must have set 'background' back to another
		 * value, that's not what we want here.  Disable the color
		 * scheme and set the colors again. */
		do_unlet((char_u *)"g:colors_name", TRUE);
		free_string_option(p_bg);
		p_bg = vim_strsave((char_u *)(dark ? "dark" : "light"));
		check_string_option(&p_bg);
		init_highlight(FALSE, FALSE);
	    }
#endif
	}
	else
	    errmsg = e_invarg;
    }

    /* 'wildmode' */
    else if (varp == &p_wim)
    {
	if (check_opt_wim() == FAIL)
	    errmsg = e_invarg;
    }

#ifdef FEAT_CMDL_COMPL
    /* 'wildoptions' */
    else if (varp == &p_wop)
    {
	if (check_opt_strings(p_wop, p_wop_values, TRUE) != OK)
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_WAK
    /* 'winaltkeys' */
    else if (varp == &p_wak)
    {
	if (*p_wak == NUL
		|| check_opt_strings(p_wak, p_wak_values, FALSE) != OK)
	    errmsg = e_invarg;
# ifdef FEAT_MENU
#  ifdef FEAT_GUI_MOTIF
	else if (gui.in_use)
	    gui_motif_set_mnemonics(p_wak[0] == 'y' || p_wak[0] == 'm');
#  else
#   ifdef FEAT_GUI_GTK
	else if (gui.in_use)
	    gui_gtk_set_mnemonics(p_wak[0] == 'y' || p_wak[0] == 'm');
#   endif
#  endif
# endif
    }
#endif

#ifdef FEAT_AUTOCMD
    /* 'eventignore' */
    else if (varp == &p_ei)
    {
	if (check_ei() == FAIL)
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_MBYTE
    /* 'encoding' and 'fileencoding' */
    else if (varp == &p_enc || gvarp == &p_fenc || varp == &p_tenc)
    {
	if (gvarp == &p_fenc)
	{
	    if (!curbuf->b_p_ma && opt_flags != OPT_GLOBAL)
		errmsg = e_modifiable;
	    else if (vim_strchr(*varp, ',') != NULL)
		/* No comma allowed in 'fileencoding'; catches confusing it
		 * with 'fileencodings'. */
		errmsg = e_invarg;
	    else
	    {
# ifdef FEAT_TITLE
		/* May show a "+" in the title now. */
		redraw_titles();
# endif
		/* Add 'fileencoding' to the swap file. */
		ml_setflags(curbuf);
	    }
	}
	if (errmsg == NULL)
	{
	    /* canonize the value, so that STRCMP() can be used on it */
	    p = enc_canonize(*varp);
	    if (p != NULL)
	    {
		vim_free(*varp);
		*varp = p;
	    }
	    if (varp == &p_enc)
	    {
		errmsg = mb_init();
# ifdef FEAT_TITLE
		redraw_titles();
# endif
	    }
	}

# if defined(FEAT_GUI_GTK)
	if (errmsg == NULL && varp == &p_tenc && gui.in_use)
	{
	    /* GTK+ 2 uses only a single encoding, and that is UTF-8. */
	    if (STRCMP(p_tenc, "utf-8") != 0)
		errmsg = (char_u *)N_("E617: Cannot be changed in the GTK+ 2 GUI");
	}
# endif

	if (errmsg == NULL)
	{
# ifdef FEAT_KEYMAP
	    /* When 'keymap' is used and 'encoding' changes, reload the keymap
	     * (with another encoding). */
	    if (varp == &p_enc && *curbuf->b_p_keymap != NUL)
		(void)keymap_init();
# endif

	    /* When 'termencoding' is not empty and 'encoding' changes or when
	     * 'termencoding' changes, need to setup for keyboard input and
	     * display output conversion. */
	    if (((varp == &p_enc && *p_tenc != NUL) || varp == &p_tenc))
	    {
		convert_setup(&input_conv, p_tenc, p_enc);
		convert_setup(&output_conv, p_enc, p_tenc);
	    }

# if defined(WIN3264) && defined(FEAT_MBYTE)
	    /* $HOME may have characters in active code page. */
	    if (varp == &p_enc)
		init_homedir();
# endif
	}
    }
#endif

#if defined(FEAT_POSTSCRIPT)
    else if (varp == &p_penc)
    {
	/* Canonize printencoding if VIM standard one */
	p = enc_canonize(p_penc);
	if (p != NULL)
	{
	    vim_free(p_penc);
	    p_penc = p;
	}
	else
	{
	    /* Ensure lower case and '-' for '_' */
	    for (s = p_penc; *s != NUL; s++)
	    {
		if (*s == '_')
		    *s = '-';
		else
		    *s = TOLOWER_ASC(*s);
	    }
	}
    }
#endif

#if defined(FEAT_XIM) && defined(FEAT_GUI_GTK)
    else if (varp == &p_imak)
    {
	if (gui.in_use && !im_xim_isvalid_imactivate())
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_KEYMAP
    else if (varp == &curbuf->b_p_keymap)
    {
	if (!valid_filetype(*varp))
	    errmsg = e_invarg;
	else
	    /* load or unload key mapping tables */
	    errmsg = keymap_init();

	if (errmsg == NULL)
	{
	    if (*curbuf->b_p_keymap != NUL)
	    {
		/* Installed a new keymap, switch on using it. */
		curbuf->b_p_iminsert = B_IMODE_LMAP;
		if (curbuf->b_p_imsearch != B_IMODE_USE_INSERT)
		    curbuf->b_p_imsearch = B_IMODE_LMAP;
	    }
	    else
	    {
		/* Cleared the keymap, may reset 'iminsert' and 'imsearch'. */
		if (curbuf->b_p_iminsert == B_IMODE_LMAP)
		    curbuf->b_p_iminsert = B_IMODE_NONE;
		if (curbuf->b_p_imsearch == B_IMODE_LMAP)
		    curbuf->b_p_imsearch = B_IMODE_USE_INSERT;
	    }
	    if ((opt_flags & OPT_LOCAL) == 0)
	    {
		set_iminsert_global();
		set_imsearch_global();
	    }
# ifdef FEAT_WINDOWS
	    status_redraw_curbuf();
# endif
	}
    }
#endif

    /* 'fileformat' */
    else if (gvarp == &p_ff)
    {
	if (!curbuf->b_p_ma && !(opt_flags & OPT_GLOBAL))
	    errmsg = e_modifiable;
	else if (check_opt_strings(*varp, p_ff_values, FALSE) != OK)
	    errmsg = e_invarg;
	else
	{
	    /* may also change 'textmode' */
	    if (get_fileformat(curbuf) == EOL_DOS)
		curbuf->b_p_tx = TRUE;
	    else
		curbuf->b_p_tx = FALSE;
#ifdef FEAT_TITLE
	    redraw_titles();
#endif
	    /* update flag in swap file */
	    ml_setflags(curbuf);
	    /* Redraw needed when switching to/from "mac": a CR in the text
	     * will be displayed differently. */
	    if (get_fileformat(curbuf) == EOL_MAC || *oldval == 'm')
		redraw_curbuf_later(NOT_VALID);
	}
    }

    /* 'fileformats' */
    else if (varp == &p_ffs)
    {
	if (check_opt_strings(p_ffs, p_ff_values, TRUE) != OK)
	    errmsg = e_invarg;
	else
	{
	    /* also change 'textauto' */
	    if (*p_ffs == NUL)
		p_ta = FALSE;
	    else
		p_ta = TRUE;
	}
    }

#if defined(FEAT_CRYPT)
    /* 'cryptkey' */
    else if (gvarp == &p_key)
    {
# if defined(FEAT_CMDHIST)
	/* Make sure the ":set" command doesn't show the new value in the
	 * history. */
	remove_key_from_history();
# endif
	if (STRCMP(curbuf->b_p_key, oldval) != 0)
	    /* Need to update the swapfile. */
	    ml_set_crypt_key(curbuf, oldval,
			      *curbuf->b_p_cm == NUL ? p_cm : curbuf->b_p_cm);
    }

    else if (gvarp == &p_cm)
    {
	if (opt_flags & OPT_LOCAL)
	    p = curbuf->b_p_cm;
	else
	    p = p_cm;
	if (check_opt_strings(p, p_cm_values, TRUE) != OK)
	    errmsg = e_invarg;
	else if (crypt_self_test() == FAIL)
	    errmsg = e_invarg;
	else
	{
	    /* When setting the global value to empty, make it "zip". */
	    if (*p_cm == NUL)
	    {
		if (new_value_alloced)
		    free_string_option(p_cm);
		p_cm = vim_strsave((char_u *)"zip");
		new_value_alloced = TRUE;
	    }
	    /* When using ":set cm=name" the local value is going to be empty.
	     * Do that here, otherwise the crypt functions will still use the
	     * local value. */
	    if ((opt_flags & (OPT_LOCAL | OPT_GLOBAL)) == 0)
	    {
		free_string_option(curbuf->b_p_cm);
		curbuf->b_p_cm = empty_option;
	    }

	    /* Need to update the swapfile when the effective method changed.
	     * Set "s" to the effective old value, "p" to the effective new
	     * method and compare. */
	    if ((opt_flags & OPT_LOCAL) && *oldval == NUL)
		s = p_cm;  /* was previously using the global value */
	    else
		s = oldval;
	    if (*curbuf->b_p_cm == NUL)
		p = p_cm;  /* is now using the global value */
	    else
		p = curbuf->b_p_cm;
	    if (STRCMP(s, p) != 0)
		ml_set_crypt_key(curbuf, curbuf->b_p_key, s);

	    /* If the global value changes need to update the swapfile for all
	     * buffers using that value. */
	    if ((opt_flags & OPT_GLOBAL) && STRCMP(p_cm, oldval) != 0)
	    {
		buf_T	*buf;

		FOR_ALL_BUFFERS(buf)
		    if (buf != curbuf && *buf->b_p_cm == NUL)
			ml_set_crypt_key(buf, buf->b_p_key, oldval);
	    }
	}
    }
#endif

    /* 'matchpairs' */
    else if (gvarp == &p_mps)
    {
#ifdef FEAT_MBYTE
	if (has_mbyte)
	{
	    for (p = *varp; *p != NUL; ++p)
	    {
		int x2 = -1;
		int x3 = -1;

		if (*p != NUL)
		    p += mb_ptr2len(p);
		if (*p != NUL)
		    x2 = *p++;
		if (*p != NUL)
		{
		    x3 = mb_ptr2char(p);
		    p += mb_ptr2len(p);
		}
		if (x2 != ':' || x3 == -1 || (*p != NUL && *p != ','))
		{
		    errmsg = e_invarg;
		    break;
		}
		if (*p == NUL)
		    break;
	    }
	}
	else
#endif
	{
	    /* Check for "x:y,x:y" */
	    for (p = *varp; *p != NUL; p += 4)
	    {
		if (p[1] != ':' || p[2] == NUL || (p[3] != NUL && p[3] != ','))
		{
		    errmsg = e_invarg;
		    break;
		}
		if (p[3] == NUL)
		    break;
	    }
	}
    }

#ifdef FEAT_COMMENTS
    /* 'comments' */
    else if (gvarp == &p_com)
    {
	for (s = *varp; *s; )
	{
	    while (*s && *s != ':')
	    {
		if (vim_strchr((char_u *)COM_ALL, *s) == NULL
					     && !VIM_ISDIGIT(*s) && *s != '-')
		{
		    errmsg = illegal_char(errbuf, *s);
		    break;
		}
		++s;
	    }
	    if (*s++ == NUL)
		errmsg = (char_u *)N_("E524: Missing colon");
	    else if (*s == ',' || *s == NUL)
		errmsg = (char_u *)N_("E525: Zero length string");
	    if (errmsg != NULL)
		break;
	    while (*s && *s != ',')
	    {
		if (*s == '\\' && s[1] != NUL)
		    ++s;
		++s;
	    }
	    s = skip_to_option_part(s);
	}
    }
#endif

    /* 'listchars' */
    else if (varp == &p_lcs)
    {
	errmsg = set_chars_option(varp);
    }

#if defined(FEAT_WINDOWS) || defined(FEAT_FOLDING)
    /* 'fillchars' */
    else if (varp == &p_fcs)
    {
	errmsg = set_chars_option(varp);
    }
#endif

#ifdef FEAT_CMDWIN
    /* 'cedit' */
    else if (varp == &p_cedit)
    {
	errmsg = check_cedit();
    }
#endif

    /* 'verbosefile' */
    else if (varp == &p_vfile)
    {
	verbose_stop();
	if (*p_vfile != NUL && verbose_open() == FAIL)
	    errmsg = e_invarg;
    }

#ifdef FEAT_VIMINFO
    /* 'viminfo' */
    else if (varp == &p_viminfo)
    {
	for (s = p_viminfo; *s;)
	{
	    /* Check it's a valid character */
	    if (vim_strchr((char_u *)"!\"%'/:<@cfhnrs", *s) == NULL)
	    {
		errmsg = illegal_char(errbuf, *s);
		break;
	    }
	    if (*s == 'n')	/* name is always last one */
	    {
		break;
	    }
	    else if (*s == 'r') /* skip until next ',' */
	    {
		while (*++s && *s != ',')
		    ;
	    }
	    else if (*s == '%')
	    {
		/* optional number */
		while (vim_isdigit(*++s))
		    ;
	    }
	    else if (*s == '!' || *s == 'h' || *s == 'c')
		++s;		/* no extra chars */
	    else		/* must have a number */
	    {
		while (vim_isdigit(*++s))
		    ;

		if (!VIM_ISDIGIT(*(s - 1)))
		{
		    if (errbuf != NULL)
		    {
			sprintf((char *)errbuf,
					 _("E526: Missing number after <%s>"),
						    transchar_byte(*(s - 1)));
			errmsg = errbuf;
		    }
		    else
			errmsg = (char_u *)"";
		    break;
		}
	    }
	    if (*s == ',')
		++s;
	    else if (*s)
	    {
		if (errbuf != NULL)
		    errmsg = (char_u *)N_("E527: Missing comma");
		else
		    errmsg = (char_u *)"";
		break;
	    }
	}
	if (*p_viminfo && errmsg == NULL && get_viminfo_parameter('\'') < 0)
	    errmsg = (char_u *)N_("E528: Must specify a ' value");
    }
#endif /* FEAT_VIMINFO */

    /* terminal options */
    else if (istermoption(&options[opt_idx]) && full_screen)
    {
	/* ":set t_Co=0" and ":set t_Co=1" do ":set t_Co=" */
	if (varp == &T_CCO)
	{
	    int colors = atoi((char *)T_CCO);

	    /* Only reinitialize colors if t_Co value has really changed to
	     * avoid expensive reload of colorscheme if t_Co is set to the
	     * same value multiple times. */
	    if (colors != t_colors)
	    {
		t_colors = colors;
		if (t_colors <= 1)
		{
		    if (new_value_alloced)
			vim_free(T_CCO);
		    T_CCO = empty_option;
		}
		/* We now have a different color setup, initialize it again. */
		init_highlight(TRUE, FALSE);
	    }
	}
	ttest(FALSE);
	if (varp == &T_ME)
	{
	    out_str(T_ME);
	    redraw_later(CLEAR);
#if defined(WIN3264) && !defined(FEAT_GUI_W32)
	    /* Since t_me has been set, this probably means that the user
	     * wants to use this as default colors.  Need to reset default
	     * background/foreground colors. */
	    mch_set_normal_colors();
#endif
	}
    }

#ifdef FEAT_LINEBREAK
    /* 'showbreak' */
    else if (varp == &p_sbr)
    {
	for (s = p_sbr; *s; )
	{
	    if (ptr2cells(s) != 1)
		errmsg = (char_u *)N_("E595: contains unprintable or wide character");
	    mb_ptr_adv(s);
	}
    }
#endif

#ifdef FEAT_GUI
    /* 'guifont' */
    else if (varp == &p_guifont)
    {
	if (gui.in_use)
	{
	    p = p_guifont;
# if defined(FEAT_GUI_GTK)
	    /*
	     * Put up a font dialog and let the user select a new value.
	     * If this is cancelled go back to the old value but don't
	     * give an error message.
	     */
	    if (STRCMP(p, "*") == 0)
	    {
		p = gui_mch_font_dialog(oldval);

		if (new_value_alloced)
		    free_string_option(p_guifont);

		p_guifont = (p != NULL) ? p : vim_strsave(oldval);
		new_value_alloced = TRUE;
	    }
# endif
	    if (p != NULL && gui_init_font(p_guifont, FALSE) != OK)
	    {
# if defined(FEAT_GUI_MSWIN) || defined(FEAT_GUI_PHOTON)
		if (STRCMP(p_guifont, "*") == 0)
		{
		    /* Dialog was cancelled: Keep the old value without giving
		     * an error message. */
		    if (new_value_alloced)
			free_string_option(p_guifont);
		    p_guifont = vim_strsave(oldval);
		    new_value_alloced = TRUE;
		}
		else
# endif
		    errmsg = (char_u *)N_("E596: Invalid font(s)");
	    }
	}
	redraw_gui_only = TRUE;
    }
# ifdef FEAT_XFONTSET
    else if (varp == &p_guifontset)
    {
	if (STRCMP(p_guifontset, "*") == 0)
	    errmsg = (char_u *)N_("E597: can't select fontset");
	else if (gui.in_use && gui_init_font(p_guifontset, TRUE) != OK)
	    errmsg = (char_u *)N_("E598: Invalid fontset");
	redraw_gui_only = TRUE;
    }
# endif
# ifdef FEAT_MBYTE
    else if (varp == &p_guifontwide)
    {
	if (STRCMP(p_guifontwide, "*") == 0)
	    errmsg = (char_u *)N_("E533: can't select wide font");
	else if (gui_get_wide_font() == FAIL)
	    errmsg = (char_u *)N_("E534: Invalid wide font");
	redraw_gui_only = TRUE;
    }
# endif
#endif

#ifdef CURSOR_SHAPE
    /* 'guicursor' */
    else if (varp == &p_guicursor)
	errmsg = parse_shape_opt(SHAPE_CURSOR);
#endif

#ifdef FEAT_MOUSESHAPE
    /* 'mouseshape' */
    else if (varp == &p_mouseshape)
    {
	errmsg = parse_shape_opt(SHAPE_MOUSE);
	update_mouseshape(-1);
    }
#endif

#ifdef FEAT_PRINTER
    else if (varp == &p_popt)
	errmsg = parse_printoptions();
# if defined(FEAT_MBYTE) && defined(FEAT_POSTSCRIPT)
    else if (varp == &p_pmfn)
	errmsg = parse_printmbfont();
# endif
#endif

#ifdef FEAT_LANGMAP
    /* 'langmap' */
    else if (varp == &p_langmap)
	langmap_set();
#endif

#ifdef FEAT_LINEBREAK
    /* 'breakat' */
    else if (varp == &p_breakat)
	fill_breakat_flags();
#endif

#ifdef FEAT_TITLE
    /* 'titlestring' and 'iconstring' */
    else if (varp == &p_titlestring || varp == &p_iconstring)
    {
# ifdef FEAT_STL_OPT
	int	flagval = (varp == &p_titlestring) ? STL_IN_TITLE : STL_IN_ICON;

	/* NULL => statusline syntax */
	if (vim_strchr(*varp, '%') && check_stl_option(*varp) == NULL)
	    stl_syntax |= flagval;
	else
	    stl_syntax &= ~flagval;
# endif
	did_set_title(varp == &p_iconstring);

    }
#endif

#ifdef FEAT_GUI
    /* 'guioptions' */
    else if (varp == &p_go)
    {
	gui_init_which_components(oldval);
	redraw_gui_only = TRUE;
    }
#endif

#if defined(FEAT_GUI_TABLINE)
    /* 'guitablabel' */
    else if (varp == &p_gtl)
    {
	redraw_tabline = TRUE;
	redraw_gui_only = TRUE;
    }
    /* 'guitabtooltip' */
    else if (varp == &p_gtt)
    {
	redraw_gui_only = TRUE;
    }
#endif

#if defined(FEAT_MOUSE_TTY) && (defined(UNIX) || defined(VMS))
    /* 'ttymouse' */
    else if (varp == &p_ttym)
    {
	/* Switch the mouse off before changing the escape sequences used for
	 * that. */
	mch_setmouse(FALSE);
	if (opt_strings_flags(p_ttym, p_ttym_values, &ttym_flags, FALSE) != OK)
	    errmsg = e_invarg;
	else
	    check_mouse_termcode();
	if (termcap_active)
	    setmouse();		/* may switch it on again */
    }
#endif

    /* 'selection' */
    else if (varp == &p_sel)
    {
	if (*p_sel == NUL
		|| check_opt_strings(p_sel, p_sel_values, FALSE) != OK)
	    errmsg = e_invarg;
    }

    /* 'selectmode' */
    else if (varp == &p_slm)
    {
	if (check_opt_strings(p_slm, p_slm_values, TRUE) != OK)
	    errmsg = e_invarg;
    }

#ifdef FEAT_BROWSE
    /* 'browsedir' */
    else if (varp == &p_bsdir)
    {
	if (check_opt_strings(p_bsdir, p_bsdir_values, FALSE) != OK
		&& !mch_isdir(p_bsdir))
	    errmsg = e_invarg;
    }
#endif

    /* 'keymodel' */
    else if (varp == &p_km)
    {
	if (check_opt_strings(p_km, p_km_values, TRUE) != OK)
	    errmsg = e_invarg;
	else
	{
	    km_stopsel = (vim_strchr(p_km, 'o') != NULL);
	    km_startsel = (vim_strchr(p_km, 'a') != NULL);
	}
    }

    /* 'mousemodel' */
    else if (varp == &p_mousem)
    {
	if (check_opt_strings(p_mousem, p_mousem_values, FALSE) != OK)
	    errmsg = e_invarg;
#if defined(FEAT_GUI_MOTIF) && defined(FEAT_MENU) && (XmVersion <= 1002)
	else if (*p_mousem != *oldval)
	    /* Changed from "extend" to "popup" or "popup_setpos" or vv: need
	     * to create or delete the popup menus. */
	    gui_motif_update_mousemodel(root_menu);
#endif
    }

    /* 'switchbuf' */
    else if (varp == &p_swb)
    {
	if (opt_strings_flags(p_swb, p_swb_values, &swb_flags, TRUE) != OK)
	    errmsg = e_invarg;
    }

    /* 'debug' */
    else if (varp == &p_debug)
    {
	if (check_opt_strings(p_debug, p_debug_values, TRUE) != OK)
	    errmsg = e_invarg;
    }

    /* 'display' */
    else if (varp == &p_dy)
    {
	if (opt_strings_flags(p_dy, p_dy_values, &dy_flags, TRUE) != OK)
	    errmsg = e_invarg;
	else
	    (void)init_chartab();

    }

#ifdef FEAT_WINDOWS
    /* 'eadirection' */
    else if (varp == &p_ead)
    {
	if (check_opt_strings(p_ead, p_ead_values, FALSE) != OK)
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_CLIPBOARD
    /* 'clipboard' */
    else if (varp == &p_cb)
	errmsg = check_clipboard_option();
#endif

#ifdef FEAT_SPELL
    /* When 'spelllang' or 'spellfile' is set and there is a window for this
     * buffer in which 'spell' is set load the wordlists. */
    else if (varp == &(curwin->w_s->b_p_spl)
	    || varp == &(curwin->w_s->b_p_spf))
    {
	errmsg = did_set_spell_option(varp == &(curwin->w_s->b_p_spf));
    }
    /* When 'spellcapcheck' is set compile the regexp program. */
    else if (varp == &(curwin->w_s->b_p_spc))
    {
	errmsg = compile_cap_prog(curwin->w_s);
    }
    /* 'spellsuggest' */
    else if (varp == &p_sps)
    {
	if (spell_check_sps() != OK)
	    errmsg = e_invarg;
    }
    /* 'mkspellmem' */
    else if (varp == &p_msm)
    {
	if (spell_check_msm() != OK)
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_QUICKFIX
    /* When 'bufhidden' is set, check for valid value. */
    else if (gvarp == &p_bh)
    {
	if (check_opt_strings(curbuf->b_p_bh, p_bufhidden_values, FALSE) != OK)
	    errmsg = e_invarg;
    }

    /* When 'buftype' is set, check for valid value. */
    else if (gvarp == &p_bt)
    {
	if (check_opt_strings(curbuf->b_p_bt, p_buftype_values, FALSE) != OK)
	    errmsg = e_invarg;
	else
	{
# ifdef FEAT_WINDOWS
	    if (curwin->w_status_height)
	    {
		curwin->w_redr_status = TRUE;
		redraw_later(VALID);
	    }
# endif
	    curbuf->b_help = (curbuf->b_p_bt[0] == 'h');
# ifdef FEAT_TITLE
	    redraw_titles();
# endif
	}
    }
#endif

#ifdef FEAT_STL_OPT
    /* 'statusline' or 'rulerformat' */
    else if (gvarp == &p_stl || varp == &p_ruf)
    {
	int wid;

	if (varp == &p_ruf)	/* reset ru_wid first */
	    ru_wid = 0;
	s = *varp;
	if (varp == &p_ruf && *s == '%')
	{
	    /* set ru_wid if 'ruf' starts with "%99(" */
	    if (*++s == '-')	/* ignore a '-' */
		s++;
	    wid = getdigits(&s);
	    if (wid && *s == '(' && (errmsg = check_stl_option(p_ruf)) == NULL)
		ru_wid = wid;
	    else
		errmsg = check_stl_option(p_ruf);
	}
	/* check 'statusline' only if it doesn't start with "%!" */
	else if (varp == &p_ruf || s[0] != '%' || s[1] != '!')
	    errmsg = check_stl_option(s);
	if (varp == &p_ruf && errmsg == NULL)
	    comp_col();
    }
#endif

#ifdef FEAT_INS_EXPAND
    /* check if it is a valid value for 'complete' -- Acevedo */
    else if (gvarp == &p_cpt)
    {
	for (s = *varp; *s;)
	{
	    while (*s == ',' || *s == ' ')
		s++;
	    if (!*s)
		break;
	    if (vim_strchr((char_u *)".wbuksid]tU", *s) == NULL)
	    {
		errmsg = illegal_char(errbuf, *s);
		break;
	    }
	    if (*++s != NUL && *s != ',' && *s != ' ')
	    {
		if (s[-1] == 'k' || s[-1] == 's')
		{
		    /* skip optional filename after 'k' and 's' */
		    while (*s && *s != ',' && *s != ' ')
		    {
			if (*s == '\\')
			    ++s;
			++s;
		    }
		}
		else
		{
		    if (errbuf != NULL)
		    {
			sprintf((char *)errbuf,
				     _("E535: Illegal character after <%c>"),
				     *--s);
			errmsg = errbuf;
		    }
		    else
			errmsg = (char_u *)"";
		    break;
		}
	    }
	}
    }

    /* 'completeopt' */
    else if (varp == &p_cot)
    {
	if (check_opt_strings(p_cot, p_cot_values, TRUE) != OK)
	    errmsg = e_invarg;
	else
	    completeopt_was_set();
    }
#endif /* FEAT_INS_EXPAND */

#ifdef FEAT_SIGNS
    /* 'signcolumn' */
    else if (varp == &curwin->w_p_scl)
    {
	if (check_opt_strings(*varp, p_scl_values, FALSE) != OK)
	    errmsg = e_invarg;
    }
#endif


#if defined(FEAT_TOOLBAR) && !defined(FEAT_GUI_W32)
    else if (varp == &p_toolbar)
    {
	if (opt_strings_flags(p_toolbar, p_toolbar_values,
			      &toolbar_flags, TRUE) != OK)
	    errmsg = e_invarg;
	else
	{
	    out_flush();
	    gui_mch_show_toolbar((toolbar_flags &
				  (TOOLBAR_TEXT | TOOLBAR_ICONS)) != 0);
	}
    }
#endif

#if defined(FEAT_TOOLBAR) && defined(FEAT_GUI_GTK)
    /* 'toolbariconsize': GTK+ 2 only */
    else if (varp == &p_tbis)
    {
	if (opt_strings_flags(p_tbis, p_tbis_values, &tbis_flags, FALSE) != OK)
	    errmsg = e_invarg;
	else
	{
	    out_flush();
	    gui_mch_show_toolbar((toolbar_flags &
				  (TOOLBAR_TEXT | TOOLBAR_ICONS)) != 0);
	}
    }
#endif

    /* 'pastetoggle': translate key codes like in a mapping */
    else if (varp == &p_pt)
    {
	if (*p_pt)
	{
	    (void)replace_termcodes(p_pt, &p, TRUE, TRUE, FALSE);
	    if (p != NULL)
	    {
		if (new_value_alloced)
		    free_string_option(p_pt);
		p_pt = p;
		new_value_alloced = TRUE;
	    }
	}
    }

    /* 'backspace' */
    else if (varp == &p_bs)
    {
	if (VIM_ISDIGIT(*p_bs))
	{
	    if (*p_bs > '2' || p_bs[1] != NUL)
		errmsg = e_invarg;
	}
	else if (check_opt_strings(p_bs, p_bs_values, TRUE) != OK)
	    errmsg = e_invarg;
    }
    else if (varp == &p_bo)
    {
	if (opt_strings_flags(p_bo, p_bo_values, &bo_flags, TRUE) != OK)
	    errmsg = e_invarg;
    }

    /* 'tagcase' */
    else if (gvarp == &p_tc)
    {
	unsigned int	*flags;

	if (opt_flags & OPT_LOCAL)
	{
	    p = curbuf->b_p_tc;
	    flags = &curbuf->b_tc_flags;
	}
	else
	{
	    p = p_tc;
	    flags = &tc_flags;
	}

	if ((opt_flags & OPT_LOCAL) && *p == NUL)
	    /* make the local value empty: use the global value */
	    *flags = 0;
	else if (*p == NUL
		|| opt_strings_flags(p, p_tc_values, flags, FALSE) != OK)
	    errmsg = e_invarg;
    }

#ifdef FEAT_MBYTE
    /* 'casemap' */
    else if (varp == &p_cmp)
    {
	if (opt_strings_flags(p_cmp, p_cmp_values, &cmp_flags, TRUE) != OK)
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_DIFF
    /* 'diffopt' */
    else if (varp == &p_dip)
    {
	if (diffopt_changed() == FAIL)
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_FOLDING
    /* 'foldmethod' */
    else if (gvarp == &curwin->w_allbuf_opt.wo_fdm)
    {
	if (check_opt_strings(*varp, p_fdm_values, FALSE) != OK
		|| *curwin->w_p_fdm == NUL)
	    errmsg = e_invarg;
	else
	{
	    foldUpdateAll(curwin);
	    if (foldmethodIsDiff(curwin))
		newFoldLevel();
	}
    }
# ifdef FEAT_EVAL
    /* 'foldexpr' */
    else if (varp == &curwin->w_p_fde)
    {
	if (foldmethodIsExpr(curwin))
	    foldUpdateAll(curwin);
    }
# endif
    /* 'foldmarker' */
    else if (gvarp == &curwin->w_allbuf_opt.wo_fmr)
    {
	p = vim_strchr(*varp, ',');
	if (p == NULL)
	    errmsg = (char_u *)N_("E536: comma required");
	else if (p == *varp || p[1] == NUL)
	    errmsg = e_invarg;
	else if (foldmethodIsMarker(curwin))
	    foldUpdateAll(curwin);
    }
    /* 'commentstring' */
    else if (gvarp == &p_cms)
    {
	if (**varp != NUL && strstr((char *)*varp, "%s") == NULL)
	    errmsg = (char_u *)N_("E537: 'commentstring' must be empty or contain %s");
    }
    /* 'foldopen' */
    else if (varp == &p_fdo)
    {
	if (opt_strings_flags(p_fdo, p_fdo_values, &fdo_flags, TRUE) != OK)
	    errmsg = e_invarg;
    }
    /* 'foldclose' */
    else if (varp == &p_fcl)
    {
	if (check_opt_strings(p_fcl, p_fcl_values, TRUE) != OK)
	    errmsg = e_invarg;
    }
    /* 'foldignore' */
    else if (gvarp == &curwin->w_allbuf_opt.wo_fdi)
    {
	if (foldmethodIsIndent(curwin))
	    foldUpdateAll(curwin);
    }
#endif

#ifdef FEAT_VIRTUALEDIT
    /* 'virtualedit' */
    else if (varp == &p_ve)
    {
	if (opt_strings_flags(p_ve, p_ve_values, &ve_flags, TRUE) != OK)
	    errmsg = e_invarg;
	else if (STRCMP(p_ve, oldval) != 0)
	{
	    /* Recompute cursor position in case the new 've' setting
	     * changes something. */
	    validate_virtcol();
	    coladvance(curwin->w_virtcol);
	}
    }
#endif

#if defined(FEAT_CSCOPE) && defined(FEAT_QUICKFIX)
    else if (varp == &p_csqf)
    {
	if (p_csqf != NULL)
	{
	    p = p_csqf;
	    while (*p != NUL)
	    {
		if (vim_strchr((char_u *)CSQF_CMDS, *p) == NULL
			|| p[1] == NUL
			|| vim_strchr((char_u *)CSQF_FLAGS, p[1]) == NULL
			|| (p[2] != NUL && p[2] != ','))
		{
		    errmsg = e_invarg;
		    break;
		}
		else if (p[2] == NUL)
		    break;
		else
		    p += 3;
	    }
	}
    }
#endif

#ifdef FEAT_CINDENT
    /* 'cinoptions' */
    else if (gvarp == &p_cino)
    {
	/* TODO: recognize errors */
	parse_cino(curbuf);
    }
#endif

#if defined(FEAT_RENDER_OPTIONS)
    else if (varp == &p_rop && gui.in_use)
    {
	if (!gui_mch_set_rendering_options(p_rop))
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_AUTOCMD
    else if (gvarp == &p_ft)
    {
	if (!valid_filetype(*varp))
	    errmsg = e_invarg;
    }
#endif

#ifdef FEAT_SYN_HL
    else if (gvarp == &p_syn)
    {
	if (!valid_filetype(*varp))
	    errmsg = e_invarg;
    }
#endif

    /* Options that are a list of flags. */
    else
    {
	p = NULL;
	if (varp == &p_ww)
	    p = (char_u *)WW_ALL;
	if (varp == &p_shm)
	    p = (char_u *)SHM_ALL;
	else if (varp == &(p_cpo))
	    p = (char_u *)CPO_ALL;
	else if (varp == &(curbuf->b_p_fo))
	    p = (char_u *)FO_ALL;
#ifdef FEAT_CONCEAL
	else if (varp == &curwin->w_p_cocu)
	    p = (char_u *)COCU_ALL;
#endif
	else if (varp == &p_mouse)
	{
#ifdef FEAT_MOUSE
	    p = (char_u *)MOUSE_ALL;
#else
	    if (*p_mouse != NUL)
		errmsg = (char_u *)N_("E538: No mouse support");
#endif
	}
#if defined(FEAT_GUI)
	else if (varp == &p_go)
	    p = (char_u *)GO_ALL;
#endif
	if (p != NULL)
	{
	    for (s = *varp; *s; ++s)
		if (vim_strchr(p, *s) == NULL)
		{
		    errmsg = illegal_char(errbuf, *s);
		    break;
		}
	}
    }

    /*
     * If error detected, restore the previous value.
     */
    if (errmsg != NULL)
    {
	if (new_value_alloced)
	    free_string_option(*varp);
	*varp = oldval;
	/*
	 * When resetting some values, need to act on it.
	 */
	if (did_chartab)
	    (void)init_chartab();
	if (varp == &p_hl)
	    (void)highlight_changed();
    }
    else
    {
#ifdef FEAT_EVAL
	/* Remember where the option was set. */
	set_option_scriptID_idx(opt_idx, opt_flags, current_SID);
#endif
	/*
	 * Free string options that are in allocated memory.
	 * Use "free_oldval", because recursiveness may change the flags under
	 * our fingers (esp. init_highlight()).
	 */
	if (free_oldval)
	    free_string_option(oldval);
	if (new_value_alloced)
	    options[opt_idx].flags |= P_ALLOCED;
	else
	    options[opt_idx].flags &= ~P_ALLOCED;

	if ((opt_flags & (OPT_LOCAL | OPT_GLOBAL)) == 0
		&& ((int)options[opt_idx].indir & PV_BOTH))
	{
	    /* global option with local value set to use global value; free
	     * the local value and make it empty */
	    p = get_varp_scope(&(options[opt_idx]), OPT_LOCAL);
	    free_string_option(*(char_u **)p);
	    *(char_u **)p = empty_option;
	}

	/* May set global value for local option. */
	else if (!(opt_flags & OPT_LOCAL) && opt_flags != OPT_GLOBAL)
	    set_string_option_global(opt_idx, varp);

#ifdef FEAT_AUTOCMD
	/*
	 * Trigger the autocommand only after setting the flags.
	 */
# ifdef FEAT_SYN_HL
	/* When 'syntax' is set, load the syntax of that name */
	if (varp == &(curbuf->b_p_syn))
	{
	    apply_autocmds(EVENT_SYNTAX, curbuf->b_p_syn,
					       curbuf->b_fname, TRUE, curbuf);
	}
# endif
	else if (varp == &(curbuf->b_p_ft))
	{
	    /* 'filetype' is set, trigger the FileType autocommand */
	    did_filetype = TRUE;
	    apply_autocmds(EVENT_FILETYPE, curbuf->b_p_ft,
					       curbuf->b_fname, TRUE, curbuf);
	}
#endif
#ifdef FEAT_SPELL
	if (varp == &(curwin->w_s->b_p_spl))
	{
	    char_u	fname[200];
	    char_u	*q = curwin->w_s->b_p_spl;

	    /* Skip the first name if it is "cjk". */
	    if (STRNCMP(q, "cjk,", 4) == 0)
		q += 4;

	    /*
	     * Source the spell/LANG.vim in 'runtimepath'.
	     * They could set 'spellcapcheck' depending on the language.
	     * Use the first name in 'spelllang' up to '_region' or
	     * '.encoding'.
	     */
	    for (p = q; *p != NUL; ++p)
		if (vim_strchr((char_u *)"_.,", *p) != NULL)
		    break;
	    vim_snprintf((char *)fname, 200, "spell/%.*s.vim", (int)(p - q), q);
	    source_runtime(fname, DIP_ALL);
	}
#endif
    }

#ifdef FEAT_MOUSE
    if (varp == &p_mouse)
    {
# ifdef FEAT_MOUSE_TTY
	if (*p_mouse == NUL)
	    mch_setmouse(FALSE);    /* switch mouse off */
	else
# endif
	    setmouse();		    /* in case 'mouse' changed */
    }
#endif

    if (curwin->w_curswant != MAXCOL
		     && (options[opt_idx].flags & (P_CURSWANT | P_RALL)) != 0)
	curwin->w_set_curswant = TRUE;

#ifdef FEAT_GUI
    /* check redraw when it's not a GUI option or the GUI is active. */
    if (!redraw_gui_only || gui.in_use)
#endif
	check_redraw(options[opt_idx].flags);

    return errmsg;
}