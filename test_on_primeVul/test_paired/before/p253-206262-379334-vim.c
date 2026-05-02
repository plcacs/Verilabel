parse_command_modifiers(
	exarg_T	    *eap,
	char	    **errormsg,
	cmdmod_T    *cmod,
	int	    skip_only)
{
    char_u  *orig_cmd = eap->cmd;
    char_u  *cmd_start = NULL;
    int	    use_plus_cmd = FALSE;
    int	    starts_with_colon = FALSE;
    int	    vim9script = in_vim9script();
    int	    has_visual_range = FALSE;

    CLEAR_POINTER(cmod);
    cmod->cmod_flags = sticky_cmdmod_flags;

    if (STRNCMP(eap->cmd, "'<,'>", 5) == 0)
    {
	// The automatically inserted Visual area range is skipped, so that
	// typing ":cmdmod cmd" in Visual mode works without having to move the
	// range to after the modififiers. The command will be
	// "'<,'>cmdmod cmd", parse "cmdmod cmd" and then put back "'<,'>"
	// before "cmd" below.
	eap->cmd += 5;
	cmd_start = eap->cmd;
	has_visual_range = TRUE;
    }

    // Repeat until no more command modifiers are found.
    for (;;)
    {
	char_u  *p;

	while (*eap->cmd == ' ' || *eap->cmd == '\t' || *eap->cmd == ':')
	{
	    if (*eap->cmd == ':')
		starts_with_colon = TRUE;
	    ++eap->cmd;
	}

	// in ex mode, an empty command (after modifiers) works like :+
	if (*eap->cmd == NUL && exmode_active
		   && (getline_equal(eap->getline, eap->cookie, getexmodeline)
		       || getline_equal(eap->getline, eap->cookie, getexline))
			&& curwin->w_cursor.lnum < curbuf->b_ml.ml_line_count)
	{
	    use_plus_cmd = TRUE;
	    if (!skip_only)
		ex_pressedreturn = TRUE;
	    break;  // no modifiers following
	}

	// ignore comment and empty lines
	if (comment_start(eap->cmd, starts_with_colon))
	{
	    // a comment ends at a NL
	    if (eap->nextcmd == NULL)
	    {
		eap->nextcmd = vim_strchr(eap->cmd, '\n');
		if (eap->nextcmd != NULL)
		    ++eap->nextcmd;
	    }
	    if (vim9script && has_cmdmod(cmod, FALSE))
		*errormsg = _(e_command_modifier_without_command);
	    return FAIL;
	}
	if (*eap->cmd == NUL)
	{
	    if (!skip_only)
	    {
		ex_pressedreturn = TRUE;
		if (vim9script && has_cmdmod(cmod, FALSE))
		    *errormsg = _(e_command_modifier_without_command);
	    }
	    return FAIL;
	}

	p = skip_range(eap->cmd, TRUE, NULL);

	// In Vim9 script a variable can shadow a command modifier:
	//   verbose = 123
	//   verbose += 123
	//   silent! verbose = func()
	//   verbose.member = 2
	//   verbose[expr] = 2
	// But not:
	//   verbose [a, b] = list
	if (vim9script)
	{
	    char_u *s, *n;

	    for (s = eap->cmd; ASCII_ISALPHA(*s); ++s)
		;
	    n = skipwhite(s);
	    if (*n == '.' || *n == '=' || (*n != NUL && n[1] == '=')
		    || *s == '[')
		break;
	}

	switch (*p)
	{
	    // When adding an entry, also modify cmd_exists().
	    case 'a':	if (!checkforcmd_noparen(&eap->cmd, "aboveleft", 3))
			    break;
			cmod->cmod_split |= WSP_ABOVE;
			continue;

	    case 'b':	if (checkforcmd_noparen(&eap->cmd, "belowright", 3))
			{
			    cmod->cmod_split |= WSP_BELOW;
			    continue;
			}
			if (checkforcmd_opt(&eap->cmd, "browse", 3, TRUE))
			{
#ifdef FEAT_BROWSE_CMD
			    cmod->cmod_flags |= CMOD_BROWSE;
#endif
			    continue;
			}
			if (!checkforcmd_noparen(&eap->cmd, "botright", 2))
			    break;
			cmod->cmod_split |= WSP_BOT;
			continue;

	    case 'c':	if (!checkforcmd_opt(&eap->cmd, "confirm", 4, TRUE))
			    break;
#if defined(FEAT_GUI_DIALOG) || defined(FEAT_CON_DIALOG)
			cmod->cmod_flags |= CMOD_CONFIRM;
#endif
			continue;

	    case 'k':	if (checkforcmd_noparen(&eap->cmd, "keepmarks", 3))
			{
			    cmod->cmod_flags |= CMOD_KEEPMARKS;
			    continue;
			}
			if (checkforcmd_noparen(&eap->cmd, "keepalt", 5))
			{
			    cmod->cmod_flags |= CMOD_KEEPALT;
			    continue;
			}
			if (checkforcmd_noparen(&eap->cmd, "keeppatterns", 5))
			{
			    cmod->cmod_flags |= CMOD_KEEPPATTERNS;
			    continue;
			}
			if (!checkforcmd_noparen(&eap->cmd, "keepjumps", 5))
			    break;
			cmod->cmod_flags |= CMOD_KEEPJUMPS;
			continue;

	    case 'f':	// only accept ":filter {pat} cmd"
			{
			    char_u  *reg_pat;
			    char_u  *nulp = NULL;
			    int	    c = 0;

			    if (!checkforcmd_noparen(&p, "filter", 4)
				    || *p == NUL
				    || (ends_excmd(*p)
#ifdef FEAT_EVAL
					// in ":filter #pat# cmd" # does not
					// start a comment
				     && (!vim9script || VIM_ISWHITE(p[1]))
#endif
				     ))
				break;
			    if (*p == '!')
			    {
				cmod->cmod_filter_force = TRUE;
				p = skipwhite(p + 1);
				if (*p == NUL || ends_excmd(*p))
				    break;
			    }
#ifdef FEAT_EVAL
			    // Avoid that "filter(arg)" is recognized.
			    if (vim9script && !VIM_ISWHITE(p[-1]))
				break;
#endif
			    if (skip_only)
				p = skip_vimgrep_pat(p, NULL, NULL);
			    else
				// NOTE: This puts a NUL after the pattern.
				p = skip_vimgrep_pat_ext(p, &reg_pat, NULL,
								    &nulp, &c);
			    if (p == NULL || *p == NUL)
				break;
			    if (!skip_only)
			    {
				cmod->cmod_filter_regmatch.regprog =
						vim_regcomp(reg_pat, RE_MAGIC);
				if (cmod->cmod_filter_regmatch.regprog == NULL)
				    break;
				// restore the character overwritten by NUL
				if (nulp != NULL)
				    *nulp = c;
			    }
			    eap->cmd = p;
			    continue;
			}

			// ":hide" and ":hide | cmd" are not modifiers
	    case 'h':	if (p != eap->cmd || !checkforcmd_noparen(&p, "hide", 3)
					       || *p == NUL || ends_excmd(*p))
			    break;
			eap->cmd = p;
			cmod->cmod_flags |= CMOD_HIDE;
			continue;

	    case 'l':	if (checkforcmd_noparen(&eap->cmd, "lockmarks", 3))
			{
			    cmod->cmod_flags |= CMOD_LOCKMARKS;
			    continue;
			}
			if (checkforcmd_noparen(&eap->cmd, "legacy", 3))
			{
			    if (ends_excmd2(p, eap->cmd))
			    {
				*errormsg =
				      _(e_legacy_must_be_followed_by_command);
				return FAIL;
			    }
			    cmod->cmod_flags |= CMOD_LEGACY;
			    continue;
			}

			if (!checkforcmd_noparen(&eap->cmd, "leftabove", 5))
			    break;
			cmod->cmod_split |= WSP_ABOVE;
			continue;

	    case 'n':	if (checkforcmd_noparen(&eap->cmd, "noautocmd", 3))
			{
			    cmod->cmod_flags |= CMOD_NOAUTOCMD;
			    continue;
			}
			if (!checkforcmd_noparen(&eap->cmd, "noswapfile", 3))
			    break;
			cmod->cmod_flags |= CMOD_NOSWAPFILE;
			continue;

	    case 'r':	if (!checkforcmd_noparen(&eap->cmd, "rightbelow", 6))
			    break;
			cmod->cmod_split |= WSP_BELOW;
			continue;

	    case 's':	if (checkforcmd_noparen(&eap->cmd, "sandbox", 3))
			{
			    cmod->cmod_flags |= CMOD_SANDBOX;
			    continue;
			}
			if (!checkforcmd_noparen(&eap->cmd, "silent", 3))
			    break;
			cmod->cmod_flags |= CMOD_SILENT;
			if (*eap->cmd == '!' && !VIM_ISWHITE(eap->cmd[-1]))
			{
			    // ":silent!", but not "silent !cmd"
			    eap->cmd = skipwhite(eap->cmd + 1);
			    cmod->cmod_flags |= CMOD_ERRSILENT;
			}
			continue;

	    case 't':	if (checkforcmd_noparen(&p, "tab", 3))
			{
			    if (!skip_only)
			    {
				long tabnr = get_address(eap, &eap->cmd,
						    ADDR_TABS, eap->skip,
						    skip_only, FALSE, 1);
				if (tabnr == MAXLNUM)
				    cmod->cmod_tab = tabpage_index(curtab) + 1;
				else
				{
				    if (tabnr < 0 || tabnr > LAST_TAB_NR)
				    {
					*errormsg = _(e_invalid_range);
					return FAIL;
				    }
				    cmod->cmod_tab = tabnr + 1;
				}
			    }
			    eap->cmd = p;
			    continue;
			}
			if (!checkforcmd_noparen(&eap->cmd, "topleft", 2))
			    break;
			cmod->cmod_split |= WSP_TOP;
			continue;

	    case 'u':	if (!checkforcmd_noparen(&eap->cmd, "unsilent", 3))
			    break;
			cmod->cmod_flags |= CMOD_UNSILENT;
			continue;

	    case 'v':	if (checkforcmd_noparen(&eap->cmd, "vertical", 4))
			{
			    cmod->cmod_split |= WSP_VERT;
			    continue;
			}
			if (checkforcmd_noparen(&eap->cmd, "vim9cmd", 4))
			{
			    if (ends_excmd2(p, eap->cmd))
			    {
				*errormsg =
				      _(e_vim9cmd_must_be_followed_by_command);
				return FAIL;
			    }
			    cmod->cmod_flags |= CMOD_VIM9CMD;
			    continue;
			}
			if (!checkforcmd_noparen(&p, "verbose", 4))
			    break;
			if (vim_isdigit(*eap->cmd))
			{
			    // zero means not set, one is verbose == 0, etc.
			    cmod->cmod_verbose = atoi((char *)eap->cmd) + 1;
			}
			else
			    cmod->cmod_verbose = 2;  // default: verbose == 1
			eap->cmd = p;
			continue;
	}
	break;
    }

    if (has_visual_range)
    {
	if (eap->cmd > cmd_start)
	{
	    // Move the '<,'> range to after the modifiers and insert a colon.
	    // Since the modifiers have been parsed put the colon on top of the
	    // space: "'<,'>mod cmd" -> "mod:'<,'>cmd
	    // Put eap->cmd after the colon.
	    if (use_plus_cmd)
	    {
		size_t len = STRLEN(cmd_start);

		// Special case: empty command uses "+":
		//  "'<,'>mods" -> "mods'<,'>+
		mch_memmove(orig_cmd, cmd_start, len);
		STRCPY(orig_cmd + len, "'<,'>+");
	    }
	    else
	    {
		mch_memmove(cmd_start - 5, cmd_start, eap->cmd - cmd_start);
		eap->cmd -= 5;
		mch_memmove(eap->cmd - 1, ":'<,'>", 6);
	    }
	}
	else
	    // No modifiers, move the pointer back.
	    // Special case: change empty command to "+".
	    if (use_plus_cmd)
		eap->cmd = (char_u *)"'<,'>+";
	    else
		eap->cmd = orig_cmd;
    }
    else if (use_plus_cmd)
	eap->cmd = (char_u *)"+";

    return OK;
}