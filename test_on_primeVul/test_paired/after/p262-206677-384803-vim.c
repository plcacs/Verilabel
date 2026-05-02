unix_expandpath(
    garray_T	*gap,
    char_u	*path,
    int		wildoff,
    int		flags,		// EW_* flags
    int		didstar)	// expanded "**" once already
{
    char_u	*buf;
    char_u	*path_end;
    char_u	*p, *s, *e;
    int		start_len = gap->ga_len;
    char_u	*pat;
    regmatch_T	regmatch;
    int		starts_with_dot;
    int		matches;
    int		len;
    int		starstar = FALSE;
    static int	stardepth = 0;	    // depth for "**" expansion

    DIR		*dirp;
    struct dirent *dp;

    // Expanding "**" may take a long time, check for CTRL-C.
    if (stardepth > 0)
    {
	ui_breakcheck();
	if (got_int)
	    return 0;
    }

    // make room for file name
    buf = alloc(STRLEN(path) + BASENAMELEN + 5);
    if (buf == NULL)
	return 0;

    /*
     * Find the first part in the path name that contains a wildcard.
     * When EW_ICASE is set every letter is considered to be a wildcard.
     * Copy it into "buf", including the preceding characters.
     */
    p = buf;
    s = buf;
    e = NULL;
    path_end = path;
    while (*path_end != NUL)
    {
	// May ignore a wildcard that has a backslash before it; it will
	// be removed by rem_backslash() or file_pat_to_reg_pat() below.
	if (path_end >= path + wildoff && rem_backslash(path_end))
	    *p++ = *path_end++;
	else if (*path_end == '/')
	{
	    if (e != NULL)
		break;
	    s = p + 1;
	}
	else if (path_end >= path + wildoff
			 && (vim_strchr((char_u *)"*?[{~$", *path_end) != NULL
			     || (!p_fic && (flags & EW_ICASE)
					  && vim_isalpha(PTR2CHAR(path_end)))))
	    e = p;
	if (has_mbyte)
	{
	    len = (*mb_ptr2len)(path_end);
	    STRNCPY(p, path_end, len);
	    p += len;
	    path_end += len;
	}
	else
	    *p++ = *path_end++;
    }
    e = p;
    *e = NUL;

    // Now we have one wildcard component between "s" and "e".
    // Remove backslashes between "wildoff" and the start of the wildcard
    // component.
    for (p = buf + wildoff; p < s; ++p)
	if (rem_backslash(p))
	{
	    STRMOVE(p, p + 1);
	    --e;
	    --s;
	}

    // Check for "**" between "s" and "e".
    for (p = s; p < e; ++p)
	if (p[0] == '*' && p[1] == '*')
	    starstar = TRUE;

    // convert the file pattern to a regexp pattern
    starts_with_dot = *s == '.';
    pat = file_pat_to_reg_pat(s, e, NULL, FALSE);
    if (pat == NULL)
    {
	vim_free(buf);
	return 0;
    }

    // compile the regexp into a program
    if (flags & EW_ICASE)
	regmatch.rm_ic = TRUE;		// 'wildignorecase' set
    else
	regmatch.rm_ic = p_fic;	// ignore case when 'fileignorecase' is set
    if (flags & (EW_NOERROR | EW_NOTWILD))
	++emsg_silent;
    regmatch.regprog = vim_regcomp(pat, RE_MAGIC);
    if (flags & (EW_NOERROR | EW_NOTWILD))
	--emsg_silent;
    vim_free(pat);

    if (regmatch.regprog == NULL && (flags & EW_NOTWILD) == 0)
    {
	vim_free(buf);
	return 0;
    }

    // If "**" is by itself, this is the first time we encounter it and more
    // is following then find matches without any directory.
    if (!didstar && stardepth < 100 && starstar && e - s == 2
							  && *path_end == '/')
    {
	STRCPY(s, path_end + 1);
	++stardepth;
	(void)unix_expandpath(gap, buf, (int)(s - buf), flags, TRUE);
	--stardepth;
    }

    // open the directory for scanning
    *s = NUL;
    dirp = opendir(*buf == NUL ? "." : (char *)buf);

    // Find all matching entries
    if (dirp != NULL)
    {
	for (;;)
	{
	    dp = readdir(dirp);
	    if (dp == NULL)
		break;
	    if ((dp->d_name[0] != '.' || starts_with_dot
			|| ((flags & EW_DODOT)
			    && dp->d_name[1] != NUL
			    && (dp->d_name[1] != '.' || dp->d_name[2] != NUL)))
		 && ((regmatch.regprog != NULL && vim_regexec(&regmatch,
					     (char_u *)dp->d_name, (colnr_T)0))
		   || ((flags & EW_NOTWILD)
		     && fnamencmp(path + (s - buf), dp->d_name, e - s) == 0)))
	    {
		STRCPY(s, dp->d_name);
		len = STRLEN(buf);

		if (starstar && stardepth < 100)
		{
		    // For "**" in the pattern first go deeper in the tree to
		    // find matches.
		    STRCPY(buf + len, "/**");
		    STRCPY(buf + len + 3, path_end);
		    ++stardepth;
		    (void)unix_expandpath(gap, buf, len + 1, flags, TRUE);
		    --stardepth;
		}

		STRCPY(buf + len, path_end);
		if (mch_has_exp_wildcard(path_end)) // handle more wildcards
		{
		    // need to expand another component of the path
		    // remove backslashes for the remaining components only
		    (void)unix_expandpath(gap, buf, len + 1, flags, FALSE);
		}
		else
		{
		    stat_T  sb;

		    // no more wildcards, check if there is a match
		    // remove backslashes for the remaining components only
		    if (*path_end != NUL)
			backslash_halve(buf + len + 1);
		    // add existing file or symbolic link
		    if ((flags & EW_ALLLINKS) ? mch_lstat((char *)buf, &sb) >= 0
						      : mch_getperm(buf) >= 0)
		    {
#ifdef MACOS_CONVERT
			size_t precomp_len = STRLEN(buf)+1;
			char_u *precomp_buf =
			    mac_precompose_path(buf, precomp_len, &precomp_len);

			if (precomp_buf)
			{
			    mch_memmove(buf, precomp_buf, precomp_len);
			    vim_free(precomp_buf);
			}
#endif
			addfile(gap, buf, flags);
		    }
		}
	    }
	}

	closedir(dirp);
    }

    vim_free(buf);
    vim_regfree(regmatch.regprog);

    matches = gap->ga_len - start_len;
    if (matches > 0)
	qsort(((char_u **)gap->ga_data) + start_len, matches,
						   sizeof(char_u *), pstrcmp);
    return matches;
}