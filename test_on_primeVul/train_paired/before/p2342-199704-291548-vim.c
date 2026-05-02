spell_dump_compl(
    char_u	*pat,	    // leading part of the word
    int		ic,	    // ignore case
    int		*dir,	    // direction for adding matches
    int		dumpflags_arg)	// DUMPFLAG_*
{
    langp_T	*lp;
    slang_T	*slang;
    idx_T	arridx[MAXWLEN];
    int		curi[MAXWLEN];
    char_u	word[MAXWLEN];
    int		c;
    char_u	*byts;
    idx_T	*idxs;
    linenr_T	lnum = 0;
    int		round;
    int		depth;
    int		n;
    int		flags;
    char_u	*region_names = NULL;	    // region names being used
    int		do_region = TRUE;	    // dump region names and numbers
    char_u	*p;
    int		lpi;
    int		dumpflags = dumpflags_arg;
    int		patlen;

    // When ignoring case or when the pattern starts with capital pass this on
    // to dump_word().
    if (pat != NULL)
    {
	if (ic)
	    dumpflags |= DUMPFLAG_ICASE;
	else
	{
	    n = captype(pat, NULL);
	    if (n == WF_ONECAP)
		dumpflags |= DUMPFLAG_ONECAP;
	    else if (n == WF_ALLCAP && (int)STRLEN(pat) > mb_ptr2len(pat))
		dumpflags |= DUMPFLAG_ALLCAP;
	}
    }

    // Find out if we can support regions: All languages must support the same
    // regions or none at all.
    for (lpi = 0; lpi < curwin->w_s->b_langp.ga_len; ++lpi)
    {
	lp = LANGP_ENTRY(curwin->w_s->b_langp, lpi);
	p = lp->lp_slang->sl_regions;
	if (p[0] != 0)
	{
	    if (region_names == NULL)	    // first language with regions
		region_names = p;
	    else if (STRCMP(region_names, p) != 0)
	    {
		do_region = FALSE;	    // region names are different
		break;
	    }
	}
    }

    if (do_region && region_names != NULL)
    {
	if (pat == NULL)
	{
	    vim_snprintf((char *)IObuff, IOSIZE, "/regions=%s", region_names);
	    ml_append(lnum++, IObuff, (colnr_T)0, FALSE);
	}
    }
    else
	do_region = FALSE;

    /*
     * Loop over all files loaded for the entries in 'spelllang'.
     */
    for (lpi = 0; lpi < curwin->w_s->b_langp.ga_len; ++lpi)
    {
	lp = LANGP_ENTRY(curwin->w_s->b_langp, lpi);
	slang = lp->lp_slang;
	if (slang->sl_fbyts == NULL)	    // reloading failed
	    continue;

	if (pat == NULL)
	{
	    vim_snprintf((char *)IObuff, IOSIZE, "# file: %s", slang->sl_fname);
	    ml_append(lnum++, IObuff, (colnr_T)0, FALSE);
	}

	// When matching with a pattern and there are no prefixes only use
	// parts of the tree that match "pat".
	if (pat != NULL && slang->sl_pbyts == NULL)
	    patlen = (int)STRLEN(pat);
	else
	    patlen = -1;

	// round 1: case-folded tree
	// round 2: keep-case tree
	for (round = 1; round <= 2; ++round)
	{
	    if (round == 1)
	    {
		dumpflags &= ~DUMPFLAG_KEEPCASE;
		byts = slang->sl_fbyts;
		idxs = slang->sl_fidxs;
	    }
	    else
	    {
		dumpflags |= DUMPFLAG_KEEPCASE;
		byts = slang->sl_kbyts;
		idxs = slang->sl_kidxs;
	    }
	    if (byts == NULL)
		continue;		// array is empty

	    depth = 0;
	    arridx[0] = 0;
	    curi[0] = 1;
	    while (depth >= 0 && !got_int
				  && (pat == NULL || !ins_compl_interrupted()))
	    {
		if (curi[depth] > byts[arridx[depth]])
		{
		    // Done all bytes at this node, go up one level.
		    --depth;
		    line_breakcheck();
		    ins_compl_check_keys(50, FALSE);
		}
		else
		{
		    // Do one more byte at this node.
		    n = arridx[depth] + curi[depth];
		    ++curi[depth];
		    c = byts[n];
		    if (c == 0)
		    {
			// End of word, deal with the word.
			// Don't use keep-case words in the fold-case tree,
			// they will appear in the keep-case tree.
			// Only use the word when the region matches.
			flags = (int)idxs[n];
			if ((round == 2 || (flags & WF_KEEPCAP) == 0)
				&& (flags & WF_NEEDCOMP) == 0
				&& (do_region
				    || (flags & WF_REGION) == 0
				    || (((unsigned)flags >> 16)
						       & lp->lp_region) != 0))
			{
			    word[depth] = NUL;
			    if (!do_region)
				flags &= ~WF_REGION;

			    // Dump the basic word if there is no prefix or
			    // when it's the first one.
			    c = (unsigned)flags >> 24;
			    if (c == 0 || curi[depth] == 2)
			    {
				dump_word(slang, word, pat, dir,
						      dumpflags, flags, lnum);
				if (pat == NULL)
				    ++lnum;
			    }

			    // Apply the prefix, if there is one.
			    if (c != 0)
				lnum = dump_prefixes(slang, word, pat, dir,
						      dumpflags, flags, lnum);
			}
		    }
		    else
		    {
			// Normal char, go one level deeper.
			word[depth++] = c;
			arridx[depth] = idxs[n];
			curi[depth] = 1;

			// Check if this character matches with the pattern.
			// If not skip the whole tree below it.
			// Always ignore case here, dump_word() will check
			// proper case later.  This isn't exactly right when
			// length changes for multi-byte characters with
			// ignore case...
			if (depth <= patlen
					&& MB_STRNICMP(word, pat, depth) != 0)
			    --depth;
		    }
		}
	    }
	}
    }
}