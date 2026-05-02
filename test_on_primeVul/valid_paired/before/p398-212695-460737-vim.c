readfile(
    char_u	*fname,
    char_u	*sfname,
    linenr_T	from,
    linenr_T	lines_to_skip,
    linenr_T	lines_to_read,
    exarg_T	*eap,			/* can be NULL! */
    int		flags)
{
    int		fd = 0;
    int		newfile = (flags & READ_NEW);
    int		check_readonly;
    int		filtering = (flags & READ_FILTER);
    int		read_stdin = (flags & READ_STDIN);
    int		read_buffer = (flags & READ_BUFFER);
    int		read_fifo = (flags & READ_FIFO);
    int		set_options = newfile || read_buffer
					   || (eap != NULL && eap->read_edit);
    linenr_T	read_buf_lnum = 1;	/* next line to read from curbuf */
    colnr_T	read_buf_col = 0;	/* next char to read from this line */
    char_u	c;
    linenr_T	lnum = from;
    char_u	*ptr = NULL;		/* pointer into read buffer */
    char_u	*buffer = NULL;		/* read buffer */
    char_u	*new_buffer = NULL;	/* init to shut up gcc */
    char_u	*line_start = NULL;	/* init to shut up gcc */
    int		wasempty;		/* buffer was empty before reading */
    colnr_T	len;
    long	size = 0;
    char_u	*p;
    off_T	filesize = 0;
    int		skip_read = FALSE;
#ifdef FEAT_CRYPT
    char_u	*cryptkey = NULL;
    int		did_ask_for_key = FALSE;
#endif
#ifdef FEAT_PERSISTENT_UNDO
    context_sha256_T sha_ctx;
    int		read_undo_file = FALSE;
#endif
    int		split = 0;		/* number of split lines */
#define UNKNOWN	 0x0fffffff		/* file size is unknown */
    linenr_T	linecnt;
    int		error = FALSE;		/* errors encountered */
    int		ff_error = EOL_UNKNOWN; /* file format with errors */
    long	linerest = 0;		/* remaining chars in line */
#ifdef UNIX
    int		perm = 0;
    int		swap_mode = -1;		/* protection bits for swap file */
#else
    int		perm;
#endif
    int		fileformat = 0;		/* end-of-line format */
    int		keep_fileformat = FALSE;
    stat_T	st;
    int		file_readonly;
    linenr_T	skip_count = 0;
    linenr_T	read_count = 0;
    int		msg_save = msg_scroll;
    linenr_T	read_no_eol_lnum = 0;   /* non-zero lnum when last line of
					 * last read was missing the eol */
    int		try_mac;
    int		try_dos;
    int		try_unix;
    int		file_rewind = FALSE;
#ifdef FEAT_MBYTE
    int		can_retry;
    linenr_T	conv_error = 0;		/* line nr with conversion error */
    linenr_T	illegal_byte = 0;	/* line nr with illegal byte */
    int		keep_dest_enc = FALSE;	/* don't retry when char doesn't fit
					   in destination encoding */
    int		bad_char_behavior = BAD_REPLACE;
					/* BAD_KEEP, BAD_DROP or character to
					 * replace with */
    char_u	*tmpname = NULL;	/* name of 'charconvert' output file */
    int		fio_flags = 0;
    char_u	*fenc;			/* fileencoding to use */
    int		fenc_alloced;		/* fenc_next is in allocated memory */
    char_u	*fenc_next = NULL;	/* next item in 'fencs' or NULL */
    int		advance_fenc = FALSE;
    long	real_size = 0;
# ifdef USE_ICONV
    iconv_t	iconv_fd = (iconv_t)-1;	/* descriptor for iconv() or -1 */
#  ifdef FEAT_EVAL
    int		did_iconv = FALSE;	/* TRUE when iconv() failed and trying
					   'charconvert' next */
#  endif
# endif
    int		converted = FALSE;	/* TRUE if conversion done */
    int		notconverted = FALSE;	/* TRUE if conversion wanted but it
					   wasn't possible */
    char_u	conv_rest[CONV_RESTLEN];
    int		conv_restlen = 0;	/* nr of bytes in conv_rest[] */
#endif
#ifdef FEAT_AUTOCMD
    buf_T	*old_curbuf;
    char_u	*old_b_ffname;
    char_u	*old_b_fname;
    int		using_b_ffname;
    int		using_b_fname;
#endif

#ifdef FEAT_AUTOCMD
    au_did_filetype = FALSE; /* reset before triggering any autocommands */
#endif

    curbuf->b_no_eol_lnum = 0;	/* in case it was set by the previous read */

    /*
     * If there is no file name yet, use the one for the read file.
     * BF_NOTEDITED is set to reflect this.
     * Don't do this for a read from a filter.
     * Only do this when 'cpoptions' contains the 'f' flag.
     */
    if (curbuf->b_ffname == NULL
	    && !filtering
	    && fname != NULL
	    && vim_strchr(p_cpo, CPO_FNAMER) != NULL
	    && !(flags & READ_DUMMY))
    {
	if (set_rw_fname(fname, sfname) == FAIL)
	    return FAIL;
    }

#ifdef FEAT_AUTOCMD
    /* Remember the initial values of curbuf, curbuf->b_ffname and
     * curbuf->b_fname to detect whether they are altered as a result of
     * executing nasty autocommands.  Also check if "fname" and "sfname"
     * point to one of these values. */
    old_curbuf = curbuf;
    old_b_ffname = curbuf->b_ffname;
    old_b_fname = curbuf->b_fname;
    using_b_ffname = (fname == curbuf->b_ffname)
					      || (sfname == curbuf->b_ffname);
    using_b_fname = (fname == curbuf->b_fname) || (sfname == curbuf->b_fname);
#endif

    /* After reading a file the cursor line changes but we don't want to
     * display the line. */
    ex_no_reprint = TRUE;

    /* don't display the file info for another buffer now */
    need_fileinfo = FALSE;

    /*
     * For Unix: Use the short file name whenever possible.
     * Avoids problems with networks and when directory names are changed.
     * Don't do this for MS-DOS, a "cd" in a sub-shell may have moved us to
     * another directory, which we don't detect.
     */
    if (sfname == NULL)
	sfname = fname;
#if defined(UNIX)
    fname = sfname;
#endif

#ifdef FEAT_AUTOCMD
    /*
     * The BufReadCmd and FileReadCmd events intercept the reading process by
     * executing the associated commands instead.
     */
    if (!filtering && !read_stdin && !read_buffer)
    {
	pos_T	    pos;

	pos = curbuf->b_op_start;

	/* Set '[ mark to the line above where the lines go (line 1 if zero). */
	curbuf->b_op_start.lnum = ((from == 0) ? 1 : from);
	curbuf->b_op_start.col = 0;

	if (newfile)
	{
	    if (apply_autocmds_exarg(EVENT_BUFREADCMD, NULL, sfname,
							  FALSE, curbuf, eap))
#ifdef FEAT_EVAL
		return aborting() ? FAIL : OK;
#else
		return OK;
#endif
	}
	else if (apply_autocmds_exarg(EVENT_FILEREADCMD, sfname, sfname,
							    FALSE, NULL, eap))
#ifdef FEAT_EVAL
	    return aborting() ? FAIL : OK;
#else
	    return OK;
#endif

	curbuf->b_op_start = pos;
    }
#endif

    if ((shortmess(SHM_OVER) || curbuf->b_help) && p_verbose == 0)
	msg_scroll = FALSE;	/* overwrite previous file message */
    else
	msg_scroll = TRUE;	/* don't overwrite previous file message */

    /*
     * If the name ends in a path separator, we can't open it.  Check here,
     * because reading the file may actually work, but then creating the swap
     * file may destroy it!  Reported on MS-DOS and Win 95.
     * If the name is too long we might crash further on, quit here.
     */
    if (fname != NULL && *fname != NUL)
    {
	p = fname + STRLEN(fname);
	if (after_pathsep(fname, p) || STRLEN(fname) >= MAXPATHL)
	{
	    filemess(curbuf, fname, (char_u *)_("Illegal file name"), 0);
	    msg_end();
	    msg_scroll = msg_save;
	    return FAIL;
	}
    }

    if (!read_stdin && !read_buffer && !read_fifo)
    {
#ifdef UNIX
	/*
	 * On Unix it is possible to read a directory, so we have to
	 * check for it before the mch_open().
	 */
	perm = mch_getperm(fname);
	if (perm >= 0 && !S_ISREG(perm)		    /* not a regular file ... */
# ifdef S_ISFIFO
		      && !S_ISFIFO(perm)	    /* ... or fifo */
# endif
# ifdef S_ISSOCK
		      && !S_ISSOCK(perm)	    /* ... or socket */
# endif
# ifdef OPEN_CHR_FILES
		      && !(S_ISCHR(perm) && is_dev_fd_file(fname))
			/* ... or a character special file named /dev/fd/<n> */
# endif
						)
	{
	    int retval = FAIL;

	    if (S_ISDIR(perm))
	    {
		filemess(curbuf, fname, (char_u *)_("is a directory"), 0);
		retval = NOTDONE;
	    }
	    else
		filemess(curbuf, fname, (char_u *)_("is not a file"), 0);
	    msg_end();
	    msg_scroll = msg_save;
	    return retval;
	}
#endif
#if defined(MSWIN)
	/*
	 * MS-Windows allows opening a device, but we will probably get stuck
	 * trying to read it.
	 */
	if (!p_odev && mch_nodetype(fname) == NODE_WRITABLE)
	{
	    filemess(curbuf, fname, (char_u *)_("is a device (disabled with 'opendevice' option)"), 0);
	    msg_end();
	    msg_scroll = msg_save;
	    return FAIL;
	}
#endif
    }

    /* Set default or forced 'fileformat' and 'binary'. */
    set_file_options(set_options, eap);

    /*
     * When opening a new file we take the readonly flag from the file.
     * Default is r/w, can be set to r/o below.
     * Don't reset it when in readonly mode
     * Only set/reset b_p_ro when BF_CHECK_RO is set.
     */
    check_readonly = (newfile && (curbuf->b_flags & BF_CHECK_RO));
    if (check_readonly && !readonlymode)
	curbuf->b_p_ro = FALSE;

    if (newfile && !read_stdin && !read_buffer && !read_fifo)
    {
	/* Remember time of file. */
	if (mch_stat((char *)fname, &st) >= 0)
	{
	    buf_store_time(curbuf, &st, fname);
	    curbuf->b_mtime_read = curbuf->b_mtime;
#ifdef UNIX
	    /*
	     * Use the protection bits of the original file for the swap file.
	     * This makes it possible for others to read the name of the
	     * edited file from the swapfile, but only if they can read the
	     * edited file.
	     * Remove the "write" and "execute" bits for group and others
	     * (they must not write the swapfile).
	     * Add the "read" and "write" bits for the user, otherwise we may
	     * not be able to write to the file ourselves.
	     * Setting the bits is done below, after creating the swap file.
	     */
	    swap_mode = (st.st_mode & 0644) | 0600;
#endif
#ifdef FEAT_CW_EDITOR
	    /* Get the FSSpec on MacOS
	     * TODO: Update it properly when the buffer name changes
	     */
	    (void)GetFSSpecFromPath(curbuf->b_ffname, &curbuf->b_FSSpec);
#endif
#ifdef VMS
	    curbuf->b_fab_rfm = st.st_fab_rfm;
	    curbuf->b_fab_rat = st.st_fab_rat;
	    curbuf->b_fab_mrs = st.st_fab_mrs;
#endif
	}
	else
	{
	    curbuf->b_mtime = 0;
	    curbuf->b_mtime_read = 0;
	    curbuf->b_orig_size = 0;
	    curbuf->b_orig_mode = 0;
	}

	/* Reset the "new file" flag.  It will be set again below when the
	 * file doesn't exist. */
	curbuf->b_flags &= ~(BF_NEW | BF_NEW_W);
    }

/*
 * for UNIX: check readonly with perm and mch_access()
 * for Amiga: check readonly by trying to open the file for writing
 */
    file_readonly = FALSE;
    if (read_stdin)
    {
#if defined(MSWIN)
	/* Force binary I/O on stdin to avoid CR-LF -> LF conversion. */
	setmode(0, O_BINARY);
#endif
    }
    else if (!read_buffer)
    {
#ifdef USE_MCH_ACCESS
	if (
# ifdef UNIX
	    !(perm & 0222) ||
# endif
				mch_access((char *)fname, W_OK))
	    file_readonly = TRUE;
	fd = mch_open((char *)fname, O_RDONLY | O_EXTRA, 0);
#else
	if (!newfile
		|| readonlymode
		|| (fd = mch_open((char *)fname, O_RDWR | O_EXTRA, 0)) < 0)
	{
	    file_readonly = TRUE;
	    /* try to open ro */
	    fd = mch_open((char *)fname, O_RDONLY | O_EXTRA, 0);
	}
#endif
    }

    if (fd < 0)			    /* cannot open at all */
    {
#ifndef UNIX
	int	isdir_f;
#endif
	msg_scroll = msg_save;
#ifndef UNIX
	/*
	 * On Amiga we can't open a directory, check here.
	 */
	isdir_f = (mch_isdir(fname));
	perm = mch_getperm(fname);  /* check if the file exists */
	if (isdir_f)
	{
	    filemess(curbuf, sfname, (char_u *)_("is a directory"), 0);
	    curbuf->b_p_ro = TRUE;	/* must use "w!" now */
	}
	else
#endif
	    if (newfile)
	    {
		if (perm < 0
#ifdef ENOENT
			&& errno == ENOENT
#endif
		   )
		{
		    /*
		     * Set the 'new-file' flag, so that when the file has
		     * been created by someone else, a ":w" will complain.
		     */
		    curbuf->b_flags |= BF_NEW;

		    /* Create a swap file now, so that other Vims are warned
		     * that we are editing this file.  Don't do this for a
		     * "nofile" or "nowrite" buffer type. */
#ifdef FEAT_QUICKFIX
		    if (!bt_dontwrite(curbuf))
#endif
		    {
			check_need_swap(newfile);
#ifdef FEAT_AUTOCMD
			/* SwapExists autocommand may mess things up */
			if (curbuf != old_curbuf
				|| (using_b_ffname
					&& (old_b_ffname != curbuf->b_ffname))
				|| (using_b_fname
					 && (old_b_fname != curbuf->b_fname)))
			{
			    EMSG(_(e_auchangedbuf));
			    return FAIL;
			}
#endif
		    }
		    if (dir_of_file_exists(fname))
			filemess(curbuf, sfname, (char_u *)_("[New File]"), 0);
		    else
			filemess(curbuf, sfname,
					   (char_u *)_("[New DIRECTORY]"), 0);
#ifdef FEAT_VIMINFO
		    /* Even though this is a new file, it might have been
		     * edited before and deleted.  Get the old marks. */
		    check_marks_read();
#endif
#ifdef FEAT_MBYTE
		    /* Set forced 'fileencoding'.  */
		    if (eap != NULL)
			set_forced_fenc(eap);
#endif
#ifdef FEAT_AUTOCMD
		    apply_autocmds_exarg(EVENT_BUFNEWFILE, sfname, sfname,
							  FALSE, curbuf, eap);
#endif
		    /* remember the current fileformat */
		    save_file_ff(curbuf);

#if defined(FEAT_AUTOCMD) && defined(FEAT_EVAL)
		    if (aborting())   /* autocmds may abort script processing */
			return FAIL;
#endif
		    return OK;	    /* a new file is not an error */
		}
		else
		{
		    filemess(curbuf, sfname, (char_u *)(
# ifdef EFBIG
			    (errno == EFBIG) ? _("[File too big]") :
# endif
# ifdef EOVERFLOW
			    (errno == EOVERFLOW) ? _("[File too big]") :
# endif
						_("[Permission Denied]")), 0);
		    curbuf->b_p_ro = TRUE;	/* must use "w!" now */
		}
	    }

	return FAIL;
    }

    /*
     * Only set the 'ro' flag for readonly files the first time they are
     * loaded.	Help files always get readonly mode
     */
    if ((check_readonly && file_readonly) || curbuf->b_help)
	curbuf->b_p_ro = TRUE;

    if (set_options)
    {
	/* Don't change 'eol' if reading from buffer as it will already be
	 * correctly set when reading stdin. */
	if (!read_buffer)
	{
	    curbuf->b_p_eol = TRUE;
	    curbuf->b_start_eol = TRUE;
	}
#ifdef FEAT_MBYTE
	curbuf->b_p_bomb = FALSE;
	curbuf->b_start_bomb = FALSE;
#endif
    }

    /* Create a swap file now, so that other Vims are warned that we are
     * editing this file.
     * Don't do this for a "nofile" or "nowrite" buffer type. */
#ifdef FEAT_QUICKFIX
    if (!bt_dontwrite(curbuf))
#endif
    {
	check_need_swap(newfile);
#ifdef FEAT_AUTOCMD
	if (!read_stdin && (curbuf != old_curbuf
		|| (using_b_ffname && (old_b_ffname != curbuf->b_ffname))
		|| (using_b_fname && (old_b_fname != curbuf->b_fname))))
	{
	    EMSG(_(e_auchangedbuf));
	    if (!read_buffer)
		close(fd);
	    return FAIL;
	}
#endif
#ifdef UNIX
	/* Set swap file protection bits after creating it. */
	if (swap_mode > 0 && curbuf->b_ml.ml_mfp != NULL
			  && curbuf->b_ml.ml_mfp->mf_fname != NULL)
	    (void)mch_setperm(curbuf->b_ml.ml_mfp->mf_fname, (long)swap_mode);
#endif
    }

#if defined(HAS_SWAP_EXISTS_ACTION)
    /* If "Quit" selected at ATTENTION dialog, don't load the file */
    if (swap_exists_action == SEA_QUIT)
    {
	if (!read_buffer && !read_stdin)
	    close(fd);
	return FAIL;
    }
#endif

    ++no_wait_return;	    /* don't wait for return yet */

    /*
     * Set '[ mark to the line above where the lines go (line 1 if zero).
     */
    curbuf->b_op_start.lnum = ((from == 0) ? 1 : from);
    curbuf->b_op_start.col = 0;

    try_mac = (vim_strchr(p_ffs, 'm') != NULL);
    try_dos = (vim_strchr(p_ffs, 'd') != NULL);
    try_unix = (vim_strchr(p_ffs, 'x') != NULL);

#ifdef FEAT_AUTOCMD
    if (!read_buffer)
    {
	int	m = msg_scroll;
	int	n = msg_scrolled;

	/*
	 * The file must be closed again, the autocommands may want to change
	 * the file before reading it.
	 */
	if (!read_stdin)
	    close(fd);		/* ignore errors */

	/*
	 * The output from the autocommands should not overwrite anything and
	 * should not be overwritten: Set msg_scroll, restore its value if no
	 * output was done.
	 */
	msg_scroll = TRUE;
	if (filtering)
	    apply_autocmds_exarg(EVENT_FILTERREADPRE, NULL, sfname,
							  FALSE, curbuf, eap);
	else if (read_stdin)
	    apply_autocmds_exarg(EVENT_STDINREADPRE, NULL, sfname,
							  FALSE, curbuf, eap);
	else if (newfile)
	    apply_autocmds_exarg(EVENT_BUFREADPRE, NULL, sfname,
							  FALSE, curbuf, eap);
	else
	    apply_autocmds_exarg(EVENT_FILEREADPRE, sfname, sfname,
							    FALSE, NULL, eap);
	/* autocommands may have changed it */
	try_mac = (vim_strchr(p_ffs, 'm') != NULL);
	try_dos = (vim_strchr(p_ffs, 'd') != NULL);
	try_unix = (vim_strchr(p_ffs, 'x') != NULL);

	if (msg_scrolled == n)
	    msg_scroll = m;

#ifdef FEAT_EVAL
	if (aborting())	    /* autocmds may abort script processing */
	{
	    --no_wait_return;
	    msg_scroll = msg_save;
	    curbuf->b_p_ro = TRUE;	/* must use "w!" now */
	    return FAIL;
	}
#endif
	/*
	 * Don't allow the autocommands to change the current buffer.
	 * Try to re-open the file.
	 *
	 * Don't allow the autocommands to change the buffer name either
	 * (cd for example) if it invalidates fname or sfname.
	 */
	if (!read_stdin && (curbuf != old_curbuf
		|| (using_b_ffname && (old_b_ffname != curbuf->b_ffname))
		|| (using_b_fname && (old_b_fname != curbuf->b_fname))
		|| (fd = mch_open((char *)fname, O_RDONLY | O_EXTRA, 0)) < 0))
	{
	    --no_wait_return;
	    msg_scroll = msg_save;
	    if (fd < 0)
		EMSG(_("E200: *ReadPre autocommands made the file unreadable"));
	    else
		EMSG(_("E201: *ReadPre autocommands must not change current buffer"));
	    curbuf->b_p_ro = TRUE;	/* must use "w!" now */
	    return FAIL;
	}
    }
#endif /* FEAT_AUTOCMD */

    /* Autocommands may add lines to the file, need to check if it is empty */
    wasempty = (curbuf->b_ml.ml_flags & ML_EMPTY);

    if (!recoverymode && !filtering && !(flags & READ_DUMMY))
    {
	/*
	 * Show the user that we are busy reading the input.  Sometimes this
	 * may take a while.  When reading from stdin another program may
	 * still be running, don't move the cursor to the last line, unless
	 * always using the GUI.
	 */
	if (read_stdin)
	{
#ifndef ALWAYS_USE_GUI
	    mch_msg(_("Vim: Reading from stdin...\n"));
#endif
#ifdef FEAT_GUI
	    /* Also write a message in the GUI window, if there is one. */
	    if (gui.in_use && !gui.dying && !gui.starting)
	    {
		p = (char_u *)_("Reading from stdin...");
		gui_write(p, (int)STRLEN(p));
	    }
#endif
	}
	else if (!read_buffer)
	    filemess(curbuf, sfname, (char_u *)"", 0);
    }

    msg_scroll = FALSE;			/* overwrite the file message */

    /*
     * Set linecnt now, before the "retry" caused by a wrong guess for
     * fileformat, and after the autocommands, which may change them.
     */
    linecnt = curbuf->b_ml.ml_line_count;

#ifdef FEAT_MBYTE
    /* "++bad=" argument. */
    if (eap != NULL && eap->bad_char != 0)
    {
	bad_char_behavior = eap->bad_char;
	if (set_options)
	    curbuf->b_bad_char = eap->bad_char;
    }
    else
	curbuf->b_bad_char = 0;

    /*
     * Decide which 'encoding' to use or use first.
     */
    if (eap != NULL && eap->force_enc != 0)
    {
	fenc = enc_canonize(eap->cmd + eap->force_enc);
	fenc_alloced = TRUE;
	keep_dest_enc = TRUE;
    }
    else if (curbuf->b_p_bin)
    {
	fenc = (char_u *)"";		/* binary: don't convert */
	fenc_alloced = FALSE;
    }
    else if (curbuf->b_help)
    {
	char_u	    firstline[80];
	int	    fc;

	/* Help files are either utf-8 or latin1.  Try utf-8 first, if this
	 * fails it must be latin1.
	 * Always do this when 'encoding' is "utf-8".  Otherwise only do
	 * this when needed to avoid [converted] remarks all the time.
	 * It is needed when the first line contains non-ASCII characters.
	 * That is only in *.??x files. */
	fenc = (char_u *)"latin1";
	c = enc_utf8;
	if (!c && !read_stdin)
	{
	    fc = fname[STRLEN(fname) - 1];
	    if (TOLOWER_ASC(fc) == 'x')
	    {
		/* Read the first line (and a bit more).  Immediately rewind to
		 * the start of the file.  If the read() fails "len" is -1. */
		len = read_eintr(fd, firstline, 80);
		vim_lseek(fd, (off_T)0L, SEEK_SET);
		for (p = firstline; p < firstline + len; ++p)
		    if (*p >= 0x80)
		    {
			c = TRUE;
			break;
		    }
	    }
	}

	if (c)
	{
	    fenc_next = fenc;
	    fenc = (char_u *)"utf-8";

	    /* When the file is utf-8 but a character doesn't fit in
	     * 'encoding' don't retry.  In help text editing utf-8 bytes
	     * doesn't make sense. */
	    if (!enc_utf8)
		keep_dest_enc = TRUE;
	}
	fenc_alloced = FALSE;
    }
    else if (*p_fencs == NUL)
    {
	fenc = curbuf->b_p_fenc;	/* use format from buffer */
	fenc_alloced = FALSE;
    }
    else
    {
	fenc_next = p_fencs;		/* try items in 'fileencodings' */
	fenc = next_fenc(&fenc_next);
	fenc_alloced = TRUE;
    }
#endif

    /*
     * Jump back here to retry reading the file in different ways.
     * Reasons to retry:
     * - encoding conversion failed: try another one from "fenc_next"
     * - BOM detected and fenc was set, need to setup conversion
     * - "fileformat" check failed: try another
     *
     * Variables set for special retry actions:
     * "file_rewind"	Rewind the file to start reading it again.
     * "advance_fenc"	Advance "fenc" using "fenc_next".
     * "skip_read"	Re-use already read bytes (BOM detected).
     * "did_iconv"	iconv() conversion failed, try 'charconvert'.
     * "keep_fileformat" Don't reset "fileformat".
     *
     * Other status indicators:
     * "tmpname"	When != NULL did conversion with 'charconvert'.
     *			Output file has to be deleted afterwards.
     * "iconv_fd"	When != -1 did conversion with iconv().
     */
retry:

    if (file_rewind)
    {
	if (read_buffer)
	{
	    read_buf_lnum = 1;
	    read_buf_col = 0;
	}
	else if (read_stdin || vim_lseek(fd, (off_T)0L, SEEK_SET) != 0)
	{
	    /* Can't rewind the file, give up. */
	    error = TRUE;
	    goto failed;
	}
	/* Delete the previously read lines. */
	while (lnum > from)
	    ml_delete(lnum--, FALSE);
	file_rewind = FALSE;
#ifdef FEAT_MBYTE
	if (set_options)
	{
	    curbuf->b_p_bomb = FALSE;
	    curbuf->b_start_bomb = FALSE;
	}
	conv_error = 0;
#endif
    }

    /*
     * When retrying with another "fenc" and the first time "fileformat"
     * will be reset.
     */
    if (keep_fileformat)
	keep_fileformat = FALSE;
    else
    {
	if (eap != NULL && eap->force_ff != 0)
	{
	    fileformat = get_fileformat_force(curbuf, eap);
	    try_unix = try_dos = try_mac = FALSE;
	}
	else if (curbuf->b_p_bin)
	    fileformat = EOL_UNIX;		/* binary: use Unix format */
	else if (*p_ffs == NUL)
	    fileformat = get_fileformat(curbuf);/* use format from buffer */
	else
	    fileformat = EOL_UNKNOWN;		/* detect from file */
    }

#ifdef FEAT_MBYTE
# ifdef USE_ICONV
    if (iconv_fd != (iconv_t)-1)
    {
	/* aborted conversion with iconv(), close the descriptor */
	iconv_close(iconv_fd);
	iconv_fd = (iconv_t)-1;
    }
# endif

    if (advance_fenc)
    {
	/*
	 * Try the next entry in 'fileencodings'.
	 */
	advance_fenc = FALSE;

	if (eap != NULL && eap->force_enc != 0)
	{
	    /* Conversion given with "++cc=" wasn't possible, read
	     * without conversion. */
	    notconverted = TRUE;
	    conv_error = 0;
	    if (fenc_alloced)
		vim_free(fenc);
	    fenc = (char_u *)"";
	    fenc_alloced = FALSE;
	}
	else
	{
	    if (fenc_alloced)
		vim_free(fenc);
	    if (fenc_next != NULL)
	    {
		fenc = next_fenc(&fenc_next);
		fenc_alloced = (fenc_next != NULL);
	    }
	    else
	    {
		fenc = (char_u *)"";
		fenc_alloced = FALSE;
	    }
	}
	if (tmpname != NULL)
	{
	    mch_remove(tmpname);		/* delete converted file */
	    vim_free(tmpname);
	    tmpname = NULL;
	}
    }

    /*
     * Conversion may be required when the encoding of the file is different
     * from 'encoding' or 'encoding' is UTF-16, UCS-2 or UCS-4.
     */
    fio_flags = 0;
    converted = need_conversion(fenc);
    if (converted)
    {

	/* "ucs-bom" means we need to check the first bytes of the file
	 * for a BOM. */
	if (STRCMP(fenc, ENC_UCSBOM) == 0)
	    fio_flags = FIO_UCSBOM;

	/*
	 * Check if UCS-2/4 or Latin1 to UTF-8 conversion needs to be
	 * done.  This is handled below after read().  Prepare the
	 * fio_flags to avoid having to parse the string each time.
	 * Also check for Unicode to Latin1 conversion, because iconv()
	 * appears not to handle this correctly.  This works just like
	 * conversion to UTF-8 except how the resulting character is put in
	 * the buffer.
	 */
	else if (enc_utf8 || STRCMP(p_enc, "latin1") == 0)
	    fio_flags = get_fio_flags(fenc);

# ifdef WIN3264
	/*
	 * Conversion from an MS-Windows codepage to UTF-8 or another codepage
	 * is handled with MultiByteToWideChar().
	 */
	if (fio_flags == 0)
	    fio_flags = get_win_fio_flags(fenc);
# endif

# ifdef MACOS_CONVERT
	/* Conversion from Apple MacRoman to latin1 or UTF-8 */
	if (fio_flags == 0)
	    fio_flags = get_mac_fio_flags(fenc);
# endif

# ifdef USE_ICONV
	/*
	 * Try using iconv() if we can't convert internally.
	 */
	if (fio_flags == 0
#  ifdef FEAT_EVAL
		&& !did_iconv
#  endif
		)
	    iconv_fd = (iconv_t)my_iconv_open(
				  enc_utf8 ? (char_u *)"utf-8" : p_enc, fenc);
# endif

# ifdef FEAT_EVAL
	/*
	 * Use the 'charconvert' expression when conversion is required
	 * and we can't do it internally or with iconv().
	 */
	if (fio_flags == 0 && !read_stdin && !read_buffer && *p_ccv != NUL
						    && !read_fifo
#  ifdef USE_ICONV
						    && iconv_fd == (iconv_t)-1
#  endif
		)
	{
#  ifdef USE_ICONV
	    did_iconv = FALSE;
#  endif
	    /* Skip conversion when it's already done (retry for wrong
	     * "fileformat"). */
	    if (tmpname == NULL)
	    {
		tmpname = readfile_charconvert(fname, fenc, &fd);
		if (tmpname == NULL)
		{
		    /* Conversion failed.  Try another one. */
		    advance_fenc = TRUE;
		    if (fd < 0)
		    {
			/* Re-opening the original file failed! */
			EMSG(_("E202: Conversion made file unreadable!"));
			error = TRUE;
			goto failed;
		    }
		    goto retry;
		}
	    }
	}
	else
# endif
	{
	    if (fio_flags == 0
# ifdef USE_ICONV
		    && iconv_fd == (iconv_t)-1
# endif
	       )
	    {
		/* Conversion wanted but we can't.
		 * Try the next conversion in 'fileencodings' */
		advance_fenc = TRUE;
		goto retry;
	    }
	}
    }

    /* Set "can_retry" when it's possible to rewind the file and try with
     * another "fenc" value.  It's FALSE when no other "fenc" to try, reading
     * stdin or fixed at a specific encoding. */
    can_retry = (*fenc != NUL && !read_stdin && !read_fifo && !keep_dest_enc);
#endif

    if (!skip_read)
    {
	linerest = 0;
	filesize = 0;
	skip_count = lines_to_skip;
	read_count = lines_to_read;
#ifdef FEAT_MBYTE
	conv_restlen = 0;
#endif
#ifdef FEAT_PERSISTENT_UNDO
	read_undo_file = (newfile && (flags & READ_KEEP_UNDO) == 0
				  && curbuf->b_ffname != NULL
				  && curbuf->b_p_udf
				  && !filtering
				  && !read_fifo
				  && !read_stdin
				  && !read_buffer);
	if (read_undo_file)
	    sha256_start(&sha_ctx);
#endif
#ifdef FEAT_CRYPT
	if (curbuf->b_cryptstate != NULL)
	{
	    /* Need to free the state, but keep the key, don't want to ask for
	     * it again. */
	    crypt_free_state(curbuf->b_cryptstate);
	    curbuf->b_cryptstate = NULL;
	}
#endif
    }

    while (!error && !got_int)
    {
	/*
	 * We allocate as much space for the file as we can get, plus
	 * space for the old line plus room for one terminating NUL.
	 * The amount is limited by the fact that read() only can read
	 * upto max_unsigned characters (and other things).
	 */
#if VIM_SIZEOF_INT <= 2
	if (linerest >= 0x7ff0)
	{
	    ++split;
	    *ptr = NL;		    /* split line by inserting a NL */
	    size = 1;
	}
	else
#endif
	{
	    if (!skip_read)
	    {
#if VIM_SIZEOF_INT > 2
# if defined(SSIZE_MAX) && (SSIZE_MAX < 0x10000L)
		size = SSIZE_MAX;		    /* use max I/O size, 52K */
# else
		size = 0x10000L;		    /* use buffer >= 64K */
# endif
#else
		size = 0x7ff0L - linerest;	    /* limit buffer to 32K */
#endif

		for ( ; size >= 10; size = (long)((long_u)size >> 1))
		{
		    if ((new_buffer = lalloc((long_u)(size + linerest + 1),
							      FALSE)) != NULL)
			break;
		}
		if (new_buffer == NULL)
		{
		    do_outofmem_msg((long_u)(size * 2 + linerest + 1));
		    error = TRUE;
		    break;
		}
		if (linerest)	/* copy characters from the previous buffer */
		    mch_memmove(new_buffer, ptr - linerest, (size_t)linerest);
		vim_free(buffer);
		buffer = new_buffer;
		ptr = buffer + linerest;
		line_start = buffer;

#ifdef FEAT_MBYTE
		/* May need room to translate into.
		 * For iconv() we don't really know the required space, use a
		 * factor ICONV_MULT.
		 * latin1 to utf-8: 1 byte becomes up to 2 bytes
		 * utf-16 to utf-8: 2 bytes become up to 3 bytes, 4 bytes
		 * become up to 4 bytes, size must be multiple of 2
		 * ucs-2 to utf-8: 2 bytes become up to 3 bytes, size must be
		 * multiple of 2
		 * ucs-4 to utf-8: 4 bytes become up to 6 bytes, size must be
		 * multiple of 4 */
		real_size = (int)size;
# ifdef USE_ICONV
		if (iconv_fd != (iconv_t)-1)
		    size = size / ICONV_MULT;
		else
# endif
		    if (fio_flags & FIO_LATIN1)
		    size = size / 2;
		else if (fio_flags & (FIO_UCS2 | FIO_UTF16))
		    size = (size * 2 / 3) & ~1;
		else if (fio_flags & FIO_UCS4)
		    size = (size * 2 / 3) & ~3;
		else if (fio_flags == FIO_UCSBOM)
		    size = size / ICONV_MULT;	/* worst case */
# ifdef WIN3264
		else if (fio_flags & FIO_CODEPAGE)
		    size = size / ICONV_MULT;	/* also worst case */
# endif
# ifdef MACOS_CONVERT
		else if (fio_flags & FIO_MACROMAN)
		    size = size / ICONV_MULT;	/* also worst case */
# endif
#endif

#ifdef FEAT_MBYTE
		if (conv_restlen > 0)
		{
		    /* Insert unconverted bytes from previous line. */
		    mch_memmove(ptr, conv_rest, conv_restlen);
		    ptr += conv_restlen;
		    size -= conv_restlen;
		}
#endif

		if (read_buffer)
		{
		    /*
		     * Read bytes from curbuf.  Used for converting text read
		     * from stdin.
		     */
		    if (read_buf_lnum > from)
			size = 0;
		    else
		    {
			int	n, ni;
			long	tlen;

			tlen = 0;
			for (;;)
			{
			    p = ml_get(read_buf_lnum) + read_buf_col;
			    n = (int)STRLEN(p);
			    if ((int)tlen + n + 1 > size)
			    {
				/* Filled up to "size", append partial line.
				 * Change NL to NUL to reverse the effect done
				 * below. */
				n = (int)(size - tlen);
				for (ni = 0; ni < n; ++ni)
				{
				    if (p[ni] == NL)
					ptr[tlen++] = NUL;
				    else
					ptr[tlen++] = p[ni];
				}
				read_buf_col += n;
				break;
			    }
			    else
			    {
				/* Append whole line and new-line.  Change NL
				 * to NUL to reverse the effect done below. */
				for (ni = 0; ni < n; ++ni)
				{
				    if (p[ni] == NL)
					ptr[tlen++] = NUL;
				    else
					ptr[tlen++] = p[ni];
				}
				ptr[tlen++] = NL;
				read_buf_col = 0;
				if (++read_buf_lnum > from)
				{
				    /* When the last line didn't have an
				     * end-of-line don't add it now either. */
				    if (!curbuf->b_p_eol)
					--tlen;
				    size = tlen;
				    break;
				}
			    }
			}
		    }
		}
		else
		{
		    /*
		     * Read bytes from the file.
		     */
		    size = read_eintr(fd, ptr, size);
		}

#ifdef FEAT_CRYPT
		/*
		 * At start of file: Check for magic number of encryption.
		 */
		if (filesize == 0 && size > 0)
		    cryptkey = check_for_cryptkey(cryptkey, ptr, &size,
						  &filesize, newfile, sfname,
						  &did_ask_for_key);
		/*
		 * Decrypt the read bytes.  This is done before checking for
		 * EOF because the crypt layer may be buffering.
		 */
		if (cryptkey != NULL && curbuf->b_cryptstate != NULL
								   && size > 0)
		{
		    if (crypt_works_inplace(curbuf->b_cryptstate))
		    {
			crypt_decode_inplace(curbuf->b_cryptstate, ptr, size);
		    }
		    else
		    {
			char_u	*newptr = NULL;
			int	decrypted_size;

			decrypted_size = crypt_decode_alloc(
				    curbuf->b_cryptstate, ptr, size, &newptr);

			/* If the crypt layer is buffering, not producing
			 * anything yet, need to read more. */
			if (size > 0 && decrypted_size == 0)
			    continue;

			if (linerest == 0)
			{
			    /* Simple case: reuse returned buffer (may be
			     * NULL, checked later). */
			    new_buffer = newptr;
			}
			else
			{
			    long_u	new_size;

			    /* Need new buffer to add bytes carried over. */
			    new_size = (long_u)(decrypted_size + linerest + 1);
			    new_buffer = lalloc(new_size, FALSE);
			    if (new_buffer == NULL)
			    {
				do_outofmem_msg(new_size);
				error = TRUE;
				break;
			    }

			    mch_memmove(new_buffer, buffer, linerest);
			    if (newptr != NULL)
				mch_memmove(new_buffer + linerest, newptr,
							      decrypted_size);
			}

			if (new_buffer != NULL)
			{
			    vim_free(buffer);
			    buffer = new_buffer;
			    new_buffer = NULL;
			    line_start = buffer;
			    ptr = buffer + linerest;
			}
			size = decrypted_size;
		    }
		}
#endif

		if (size <= 0)
		{
		    if (size < 0)		    /* read error */
			error = TRUE;
#ifdef FEAT_MBYTE
		    else if (conv_restlen > 0)
		    {
			/*
			 * Reached end-of-file but some trailing bytes could
			 * not be converted.  Truncated file?
			 */

			/* When we did a conversion report an error. */
			if (fio_flags != 0
# ifdef USE_ICONV
				|| iconv_fd != (iconv_t)-1
# endif
			   )
			{
			    if (can_retry)
				goto rewind_retry;
			    if (conv_error == 0)
				conv_error = curbuf->b_ml.ml_line_count
								- linecnt + 1;
			}
			/* Remember the first linenr with an illegal byte */
			else if (illegal_byte == 0)
			    illegal_byte = curbuf->b_ml.ml_line_count
								- linecnt + 1;
			if (bad_char_behavior == BAD_DROP)
			{
			    *(ptr - conv_restlen) = NUL;
			    conv_restlen = 0;
			}
			else
			{
			    /* Replace the trailing bytes with the replacement
			     * character if we were converting; if we weren't,
			     * leave the UTF8 checking code to do it, as it
			     * works slightly differently. */
			    if (bad_char_behavior != BAD_KEEP && (fio_flags != 0
# ifdef USE_ICONV
				    || iconv_fd != (iconv_t)-1
# endif
			       ))
			    {
				while (conv_restlen > 0)
				{
				    *(--ptr) = bad_char_behavior;
				    --conv_restlen;
				}
			    }
			    fio_flags = 0;	/* don't convert this */
# ifdef USE_ICONV
			    if (iconv_fd != (iconv_t)-1)
			    {
				iconv_close(iconv_fd);
				iconv_fd = (iconv_t)-1;
			    }
# endif
			}
		    }
#endif
		}
	    }
	    skip_read = FALSE;

#ifdef FEAT_MBYTE
	    /*
	     * At start of file (or after crypt magic number): Check for BOM.
	     * Also check for a BOM for other Unicode encodings, but not after
	     * converting with 'charconvert' or when a BOM has already been
	     * found.
	     */
	    if ((filesize == 0
# ifdef FEAT_CRYPT
		   || (cryptkey != NULL
			&& filesize == crypt_get_header_len(
						 crypt_get_method_nr(curbuf)))
# endif
		       )
		    && (fio_flags == FIO_UCSBOM
			|| (!curbuf->b_p_bomb
			    && tmpname == NULL
			    && (*fenc == 'u' || (*fenc == NUL && enc_utf8)))))
	    {
		char_u	*ccname;
		int	blen;

		/* no BOM detection in a short file or in binary mode */
		if (size < 2 || curbuf->b_p_bin)
		    ccname = NULL;
		else
		    ccname = check_for_bom(ptr, size, &blen,
		      fio_flags == FIO_UCSBOM ? FIO_ALL : get_fio_flags(fenc));
		if (ccname != NULL)
		{
		    /* Remove BOM from the text */
		    filesize += blen;
		    size -= blen;
		    mch_memmove(ptr, ptr + blen, (size_t)size);
		    if (set_options)
		    {
			curbuf->b_p_bomb = TRUE;
			curbuf->b_start_bomb = TRUE;
		    }
		}

		if (fio_flags == FIO_UCSBOM)
		{
		    if (ccname == NULL)
		    {
			/* No BOM detected: retry with next encoding. */
			advance_fenc = TRUE;
		    }
		    else
		    {
			/* BOM detected: set "fenc" and jump back */
			if (fenc_alloced)
			    vim_free(fenc);
			fenc = ccname;
			fenc_alloced = FALSE;
		    }
		    /* retry reading without getting new bytes or rewinding */
		    skip_read = TRUE;
		    goto retry;
		}
	    }

	    /* Include not converted bytes. */
	    ptr -= conv_restlen;
	    size += conv_restlen;
	    conv_restlen = 0;
#endif
	    /*
	     * Break here for a read error or end-of-file.
	     */
	    if (size <= 0)
		break;

#ifdef FEAT_MBYTE

# ifdef USE_ICONV
	    if (iconv_fd != (iconv_t)-1)
	    {
		/*
		 * Attempt conversion of the read bytes to 'encoding' using
		 * iconv().
		 */
		const char	*fromp;
		char		*top;
		size_t		from_size;
		size_t		to_size;

		fromp = (char *)ptr;
		from_size = size;
		ptr += size;
		top = (char *)ptr;
		to_size = real_size - size;

		/*
		 * If there is conversion error or not enough room try using
		 * another conversion.  Except for when there is no
		 * alternative (help files).
		 */
		while ((iconv(iconv_fd, (void *)&fromp, &from_size,
							       &top, &to_size)
			    == (size_t)-1 && ICONV_ERRNO != ICONV_EINVAL)
						  || from_size > CONV_RESTLEN)
		{
		    if (can_retry)
			goto rewind_retry;
		    if (conv_error == 0)
			conv_error = readfile_linenr(linecnt,
							  ptr, (char_u *)top);

		    /* Deal with a bad byte and continue with the next. */
		    ++fromp;
		    --from_size;
		    if (bad_char_behavior == BAD_KEEP)
		    {
			*top++ = *(fromp - 1);
			--to_size;
		    }
		    else if (bad_char_behavior != BAD_DROP)
		    {
			*top++ = bad_char_behavior;
			--to_size;
		    }
		}

		if (from_size > 0)
		{
		    /* Some remaining characters, keep them for the next
		     * round. */
		    mch_memmove(conv_rest, (char_u *)fromp, from_size);
		    conv_restlen = (int)from_size;
		}

		/* move the linerest to before the converted characters */
		line_start = ptr - linerest;
		mch_memmove(line_start, buffer, (size_t)linerest);
		size = (long)((char_u *)top - ptr);
	    }
# endif

# ifdef WIN3264
	    if (fio_flags & FIO_CODEPAGE)
	    {
		char_u	*src, *dst;
		WCHAR	ucs2buf[3];
		int	ucs2len;
		int	codepage = FIO_GET_CP(fio_flags);
		int	bytelen;
		int	found_bad;
		char	replstr[2];

		/*
		 * Conversion from an MS-Windows codepage or UTF-8 to UTF-8 or
		 * a codepage, using standard MS-Windows functions.  This
		 * requires two steps:
		 * 1. convert from 'fileencoding' to ucs-2
		 * 2. convert from ucs-2 to 'encoding'
		 *
		 * Because there may be illegal bytes AND an incomplete byte
		 * sequence at the end, we may have to do the conversion one
		 * character at a time to get it right.
		 */

		/* Replacement string for WideCharToMultiByte(). */
		if (bad_char_behavior > 0)
		    replstr[0] = bad_char_behavior;
		else
		    replstr[0] = '?';
		replstr[1] = NUL;

		/*
		 * Move the bytes to the end of the buffer, so that we have
		 * room to put the result at the start.
		 */
		src = ptr + real_size - size;
		mch_memmove(src, ptr, size);

		/*
		 * Do the conversion.
		 */
		dst = ptr;
		size = size;
		while (size > 0)
		{
		    found_bad = FALSE;

#  ifdef CP_UTF8	/* VC 4.1 doesn't define CP_UTF8 */
		    if (codepage == CP_UTF8)
		    {
			/* Handle CP_UTF8 input ourselves to be able to handle
			 * trailing bytes properly.
			 * Get one UTF-8 character from src. */
			bytelen = (int)utf_ptr2len_len(src, size);
			if (bytelen > size)
			{
			    /* Only got some bytes of a character.  Normally
			     * it's put in "conv_rest", but if it's too long
			     * deal with it as if they were illegal bytes. */
			    if (bytelen <= CONV_RESTLEN)
				break;

			    /* weird overlong byte sequence */
			    bytelen = size;
			    found_bad = TRUE;
			}
			else
			{
			    int	    u8c = utf_ptr2char(src);

			    if (u8c > 0xffff || (*src >= 0x80 && bytelen == 1))
				found_bad = TRUE;
			    ucs2buf[0] = u8c;
			    ucs2len = 1;
			}
		    }
		    else
#  endif
		    {
			/* We don't know how long the byte sequence is, try
			 * from one to three bytes. */
			for (bytelen = 1; bytelen <= size && bytelen <= 3;
								    ++bytelen)
			{
			    ucs2len = MultiByteToWideChar(codepage,
							 MB_ERR_INVALID_CHARS,
							 (LPCSTR)src, bytelen,
								   ucs2buf, 3);
			    if (ucs2len > 0)
				break;
			}
			if (ucs2len == 0)
			{
			    /* If we have only one byte then it's probably an
			     * incomplete byte sequence.  Otherwise discard
			     * one byte as a bad character. */
			    if (size == 1)
				break;
			    found_bad = TRUE;
			    bytelen = 1;
			}
		    }

		    if (!found_bad)
		    {
			int	i;

			/* Convert "ucs2buf[ucs2len]" to 'enc' in "dst". */
			if (enc_utf8)
			{
			    /* From UCS-2 to UTF-8.  Cannot fail. */
			    for (i = 0; i < ucs2len; ++i)
				dst += utf_char2bytes(ucs2buf[i], dst);
			}
			else
			{
			    BOOL	bad = FALSE;
			    int		dstlen;

			    /* From UCS-2 to "enc_codepage".  If the
			     * conversion uses the default character "?",
			     * the data doesn't fit in this encoding. */
			    dstlen = WideCharToMultiByte(enc_codepage, 0,
				    (LPCWSTR)ucs2buf, ucs2len,
				    (LPSTR)dst, (int)(src - dst),
				    replstr, &bad);
			    if (bad)
				found_bad = TRUE;
			    else
				dst += dstlen;
			}
		    }

		    if (found_bad)
		    {
			/* Deal with bytes we can't convert. */
			if (can_retry)
			    goto rewind_retry;
			if (conv_error == 0)
			    conv_error = readfile_linenr(linecnt, ptr, dst);
			if (bad_char_behavior != BAD_DROP)
			{
			    if (bad_char_behavior == BAD_KEEP)
			    {
				mch_memmove(dst, src, bytelen);
				dst += bytelen;
			    }
			    else
				*dst++ = bad_char_behavior;
			}
		    }

		    src += bytelen;
		    size -= bytelen;
		}

		if (size > 0)
		{
		    /* An incomplete byte sequence remaining. */
		    mch_memmove(conv_rest, src, size);
		    conv_restlen = size;
		}

		/* The new size is equal to how much "dst" was advanced. */
		size = (long)(dst - ptr);
	    }
	    else
# endif
# ifdef MACOS_CONVERT
	    if (fio_flags & FIO_MACROMAN)
	    {
		/*
		 * Conversion from Apple MacRoman char encoding to UTF-8 or
		 * latin1.  This is in os_mac_conv.c.
		 */
		if (macroman2enc(ptr, &size, real_size) == FAIL)
		    goto rewind_retry;
	    }
	    else
# endif
	    if (fio_flags != 0)
	    {
		int	u8c;
		char_u	*dest;
		char_u	*tail = NULL;

		/*
		 * "enc_utf8" set: Convert Unicode or Latin1 to UTF-8.
		 * "enc_utf8" not set: Convert Unicode to Latin1.
		 * Go from end to start through the buffer, because the number
		 * of bytes may increase.
		 * "dest" points to after where the UTF-8 bytes go, "p" points
		 * to after the next character to convert.
		 */
		dest = ptr + real_size;
		if (fio_flags == FIO_LATIN1 || fio_flags == FIO_UTF8)
		{
		    p = ptr + size;
		    if (fio_flags == FIO_UTF8)
		    {
			/* Check for a trailing incomplete UTF-8 sequence */
			tail = ptr + size - 1;
			while (tail > ptr && (*tail & 0xc0) == 0x80)
			    --tail;
			if (tail + utf_byte2len(*tail) <= ptr + size)
			    tail = NULL;
			else
			    p = tail;
		    }
		}
		else if (fio_flags & (FIO_UCS2 | FIO_UTF16))
		{
		    /* Check for a trailing byte */
		    p = ptr + (size & ~1);
		    if (size & 1)
			tail = p;
		    if ((fio_flags & FIO_UTF16) && p > ptr)
		    {
			/* Check for a trailing leading word */
			if (fio_flags & FIO_ENDIAN_L)
			{
			    u8c = (*--p << 8);
			    u8c += *--p;
			}
			else
			{
			    u8c = *--p;
			    u8c += (*--p << 8);
			}
			if (u8c >= 0xd800 && u8c <= 0xdbff)
			    tail = p;
			else
			    p += 2;
		    }
		}
		else /*  FIO_UCS4 */
		{
		    /* Check for trailing 1, 2 or 3 bytes */
		    p = ptr + (size & ~3);
		    if (size & 3)
			tail = p;
		}

		/* If there is a trailing incomplete sequence move it to
		 * conv_rest[]. */
		if (tail != NULL)
		{
		    conv_restlen = (int)((ptr + size) - tail);
		    mch_memmove(conv_rest, (char_u *)tail, conv_restlen);
		    size -= conv_restlen;
		}


		while (p > ptr)
		{
		    if (fio_flags & FIO_LATIN1)
			u8c = *--p;
		    else if (fio_flags & (FIO_UCS2 | FIO_UTF16))
		    {
			if (fio_flags & FIO_ENDIAN_L)
			{
			    u8c = (*--p << 8);
			    u8c += *--p;
			}
			else
			{
			    u8c = *--p;
			    u8c += (*--p << 8);
			}
			if ((fio_flags & FIO_UTF16)
					    && u8c >= 0xdc00 && u8c <= 0xdfff)
			{
			    int u16c;

			    if (p == ptr)
			    {
				/* Missing leading word. */
				if (can_retry)
				    goto rewind_retry;
				if (conv_error == 0)
				    conv_error = readfile_linenr(linecnt,
								      ptr, p);
				if (bad_char_behavior == BAD_DROP)
				    continue;
				if (bad_char_behavior != BAD_KEEP)
				    u8c = bad_char_behavior;
			    }

			    /* found second word of double-word, get the first
			     * word and compute the resulting character */
			    if (fio_flags & FIO_ENDIAN_L)
			    {
				u16c = (*--p << 8);
				u16c += *--p;
			    }
			    else
			    {
				u16c = *--p;
				u16c += (*--p << 8);
			    }
			    u8c = 0x10000 + ((u16c & 0x3ff) << 10)
							      + (u8c & 0x3ff);

			    /* Check if the word is indeed a leading word. */
			    if (u16c < 0xd800 || u16c > 0xdbff)
			    {
				if (can_retry)
				    goto rewind_retry;
				if (conv_error == 0)
				    conv_error = readfile_linenr(linecnt,
								      ptr, p);
				if (bad_char_behavior == BAD_DROP)
				    continue;
				if (bad_char_behavior != BAD_KEEP)
				    u8c = bad_char_behavior;
			    }
			}
		    }
		    else if (fio_flags & FIO_UCS4)
		    {
			if (fio_flags & FIO_ENDIAN_L)
			{
			    u8c = (unsigned)*--p << 24;
			    u8c += (unsigned)*--p << 16;
			    u8c += (unsigned)*--p << 8;
			    u8c += *--p;
			}
			else	/* big endian */
			{
			    u8c = *--p;
			    u8c += (unsigned)*--p << 8;
			    u8c += (unsigned)*--p << 16;
			    u8c += (unsigned)*--p << 24;
			}
		    }
		    else    /* UTF-8 */
		    {
			if (*--p < 0x80)
			    u8c = *p;
			else
			{
			    len = utf_head_off(ptr, p);
			    p -= len;
			    u8c = utf_ptr2char(p);
			    if (len == 0)
			    {
				/* Not a valid UTF-8 character, retry with
				 * another fenc when possible, otherwise just
				 * report the error. */
				if (can_retry)
				    goto rewind_retry;
				if (conv_error == 0)
				    conv_error = readfile_linenr(linecnt,
								      ptr, p);
				if (bad_char_behavior == BAD_DROP)
				    continue;
				if (bad_char_behavior != BAD_KEEP)
				    u8c = bad_char_behavior;
			    }
			}
		    }
		    if (enc_utf8)	/* produce UTF-8 */
		    {
			dest -= utf_char2len(u8c);
			(void)utf_char2bytes(u8c, dest);
		    }
		    else		/* produce Latin1 */
		    {
			--dest;
			if (u8c >= 0x100)
			{
			    /* character doesn't fit in latin1, retry with
			     * another fenc when possible, otherwise just
			     * report the error. */
			    if (can_retry)
				goto rewind_retry;
			    if (conv_error == 0)
				conv_error = readfile_linenr(linecnt, ptr, p);
			    if (bad_char_behavior == BAD_DROP)
				++dest;
			    else if (bad_char_behavior == BAD_KEEP)
				*dest = u8c;
			    else if (eap != NULL && eap->bad_char != 0)
				*dest = bad_char_behavior;
			    else
				*dest = 0xBF;
			}
			else
			    *dest = u8c;
		    }
		}

		/* move the linerest to before the converted characters */
		line_start = dest - linerest;
		mch_memmove(line_start, buffer, (size_t)linerest);
		size = (long)((ptr + real_size) - dest);
		ptr = dest;
	    }
	    else if (enc_utf8 && !curbuf->b_p_bin)
	    {
		int  incomplete_tail = FALSE;

		/* Reading UTF-8: Check if the bytes are valid UTF-8. */
		for (p = ptr; ; ++p)
		{
		    int	 todo = (int)((ptr + size) - p);
		    int	 l;

		    if (todo <= 0)
			break;
		    if (*p >= 0x80)
		    {
			/* A length of 1 means it's an illegal byte.  Accept
			 * an incomplete character at the end though, the next
			 * read() will get the next bytes, we'll check it
			 * then. */
			l = utf_ptr2len_len(p, todo);
			if (l > todo && !incomplete_tail)
			{
			    /* Avoid retrying with a different encoding when
			     * a truncated file is more likely, or attempting
			     * to read the rest of an incomplete sequence when
			     * we have already done so. */
			    if (p > ptr || filesize > 0)
				incomplete_tail = TRUE;
			    /* Incomplete byte sequence, move it to conv_rest[]
			     * and try to read the rest of it, unless we've
			     * already done so. */
			    if (p > ptr)
			    {
				conv_restlen = todo;
				mch_memmove(conv_rest, p, conv_restlen);
				size -= conv_restlen;
				break;
			    }
			}
			if (l == 1 || l > todo)
			{
			    /* Illegal byte.  If we can try another encoding
			     * do that, unless at EOF where a truncated
			     * file is more likely than a conversion error. */
			    if (can_retry && !incomplete_tail)
				break;
# ifdef USE_ICONV
			    /* When we did a conversion report an error. */
			    if (iconv_fd != (iconv_t)-1 && conv_error == 0)
				conv_error = readfile_linenr(linecnt, ptr, p);
# endif
			    /* Remember the first linenr with an illegal byte */
			    if (conv_error == 0 && illegal_byte == 0)
				illegal_byte = readfile_linenr(linecnt, ptr, p);

			    /* Drop, keep or replace the bad byte. */
			    if (bad_char_behavior == BAD_DROP)
			    {
				mch_memmove(p, p + 1, todo - 1);
				--p;
				--size;
			    }
			    else if (bad_char_behavior != BAD_KEEP)
				*p = bad_char_behavior;
			}
			else
			    p += l - 1;
		    }
		}
		if (p < ptr + size && !incomplete_tail)
		{
		    /* Detected a UTF-8 error. */
rewind_retry:
		    /* Retry reading with another conversion. */
# if defined(FEAT_EVAL) && defined(USE_ICONV)
		    if (*p_ccv != NUL && iconv_fd != (iconv_t)-1)
			/* iconv() failed, try 'charconvert' */
			did_iconv = TRUE;
		    else
# endif
			/* use next item from 'fileencodings' */
			advance_fenc = TRUE;
		    file_rewind = TRUE;
		    goto retry;
		}
	    }
#endif

	    /* count the number of characters (after conversion!) */
	    filesize += size;

	    /*
	     * when reading the first part of a file: guess EOL type
	     */
	    if (fileformat == EOL_UNKNOWN)
	    {
		/* First try finding a NL, for Dos and Unix */
		if (try_dos || try_unix)
		{
		    /* Reset the carriage return counter. */
		    if (try_mac)
			try_mac = 1;

		    for (p = ptr; p < ptr + size; ++p)
		    {
			if (*p == NL)
			{
			    if (!try_unix
				    || (try_dos && p > ptr && p[-1] == CAR))
				fileformat = EOL_DOS;
			    else
				fileformat = EOL_UNIX;
			    break;
			}
			else if (*p == CAR && try_mac)
			    try_mac++;
		    }

		    /* Don't give in to EOL_UNIX if EOL_MAC is more likely */
		    if (fileformat == EOL_UNIX && try_mac)
		    {
			/* Need to reset the counters when retrying fenc. */
			try_mac = 1;
			try_unix = 1;
			for (; p >= ptr && *p != CAR; p--)
			    ;
			if (p >= ptr)
			{
			    for (p = ptr; p < ptr + size; ++p)
			    {
				if (*p == NL)
				    try_unix++;
				else if (*p == CAR)
				    try_mac++;
			    }
			    if (try_mac > try_unix)
				fileformat = EOL_MAC;
			}
		    }
		    else if (fileformat == EOL_UNKNOWN && try_mac == 1)
			/* Looking for CR but found no end-of-line markers at
			 * all: use the default format. */
			fileformat = default_fileformat();
		}

		/* No NL found: may use Mac format */
		if (fileformat == EOL_UNKNOWN && try_mac)
		    fileformat = EOL_MAC;

		/* Still nothing found?  Use first format in 'ffs' */
		if (fileformat == EOL_UNKNOWN)
		    fileformat = default_fileformat();

		/* if editing a new file: may set p_tx and p_ff */
		if (set_options)
		    set_fileformat(fileformat, OPT_LOCAL);
	    }
	}

	/*
	 * This loop is executed once for every character read.
	 * Keep it fast!
	 */
	if (fileformat == EOL_MAC)
	{
	    --ptr;
	    while (++ptr, --size >= 0)
	    {
		/* catch most common case first */
		if ((c = *ptr) != NUL && c != CAR && c != NL)
		    continue;
		if (c == NUL)
		    *ptr = NL;	/* NULs are replaced by newlines! */
		else if (c == NL)
		    *ptr = CAR;	/* NLs are replaced by CRs! */
		else
		{
		    if (skip_count == 0)
		    {
			*ptr = NUL;	    /* end of line */
			len = (colnr_T) (ptr - line_start + 1);
			if (ml_append(lnum, line_start, len, newfile) == FAIL)
			{
			    error = TRUE;
			    break;
			}
#ifdef FEAT_PERSISTENT_UNDO
			if (read_undo_file)
			    sha256_update(&sha_ctx, line_start, len);
#endif
			++lnum;
			if (--read_count == 0)
			{
			    error = TRUE;	/* break loop */
			    line_start = ptr;	/* nothing left to write */
			    break;
			}
		    }
		    else
			--skip_count;
		    line_start = ptr + 1;
		}
	    }
	}
	else
	{
	    --ptr;
	    while (++ptr, --size >= 0)
	    {
		if ((c = *ptr) != NUL && c != NL)  /* catch most common case */
		    continue;
		if (c == NUL)
		    *ptr = NL;	/* NULs are replaced by newlines! */
		else
		{
		    if (skip_count == 0)
		    {
			*ptr = NUL;		/* end of line */
			len = (colnr_T)(ptr - line_start + 1);
			if (fileformat == EOL_DOS)
			{
			    if (ptr > line_start && ptr[-1] == CAR)
			    {
				/* remove CR before NL */
				ptr[-1] = NUL;
				--len;
			    }
			    /*
			     * Reading in Dos format, but no CR-LF found!
			     * When 'fileformats' includes "unix", delete all
			     * the lines read so far and start all over again.
			     * Otherwise give an error message later.
			     */
			    else if (ff_error != EOL_DOS)
			    {
				if (   try_unix
				    && !read_stdin
				    && (read_buffer
					|| vim_lseek(fd, (off_T)0L, SEEK_SET)
									  == 0))
				{
				    fileformat = EOL_UNIX;
				    if (set_options)
					set_fileformat(EOL_UNIX, OPT_LOCAL);
				    file_rewind = TRUE;
				    keep_fileformat = TRUE;
				    goto retry;
				}
				ff_error = EOL_DOS;
			    }
			}
			if (ml_append(lnum, line_start, len, newfile) == FAIL)
			{
			    error = TRUE;
			    break;
			}
#ifdef FEAT_PERSISTENT_UNDO
			if (read_undo_file)
			    sha256_update(&sha_ctx, line_start, len);
#endif
			++lnum;
			if (--read_count == 0)
			{
			    error = TRUE;	    /* break loop */
			    line_start = ptr;	/* nothing left to write */
			    break;
			}
		    }
		    else
			--skip_count;
		    line_start = ptr + 1;
		}
	    }
	}
	linerest = (long)(ptr - line_start);
	ui_breakcheck();
    }

failed:
    /* not an error, max. number of lines reached */
    if (error && read_count == 0)
	error = FALSE;

    /*
     * If we get EOF in the middle of a line, note the fact and
     * complete the line ourselves.
     * In Dos format ignore a trailing CTRL-Z, unless 'binary' set.
     */
    if (!error
	    && !got_int
	    && linerest != 0
	    && !(!curbuf->b_p_bin
		&& fileformat == EOL_DOS
		&& *line_start == Ctrl_Z
		&& ptr == line_start + 1))
    {
	/* remember for when writing */
	if (set_options)
	    curbuf->b_p_eol = FALSE;
	*ptr = NUL;
	len = (colnr_T)(ptr - line_start + 1);
	if (ml_append(lnum, line_start, len, newfile) == FAIL)
	    error = TRUE;
	else
	{
#ifdef FEAT_PERSISTENT_UNDO
	    if (read_undo_file)
		sha256_update(&sha_ctx, line_start, len);
#endif
	    read_no_eol_lnum = ++lnum;
	}
    }

    if (set_options)
	save_file_ff(curbuf);		/* remember the current file format */

#ifdef FEAT_CRYPT
    if (curbuf->b_cryptstate != NULL)
    {
	crypt_free_state(curbuf->b_cryptstate);
	curbuf->b_cryptstate = NULL;
    }
    if (cryptkey != NULL && cryptkey != curbuf->b_p_key)
	crypt_free_key(cryptkey);
    /* Don't set cryptkey to NULL, it's used below as a flag that
     * encryption was used. */
#endif

#ifdef FEAT_MBYTE
    /* If editing a new file: set 'fenc' for the current buffer.
     * Also for ":read ++edit file". */
    if (set_options)
	set_string_option_direct((char_u *)"fenc", -1, fenc,
						       OPT_FREE|OPT_LOCAL, 0);
    if (fenc_alloced)
	vim_free(fenc);
# ifdef USE_ICONV
    if (iconv_fd != (iconv_t)-1)
    {
	iconv_close(iconv_fd);
	iconv_fd = (iconv_t)-1;
    }
# endif
#endif

    if (!read_buffer && !read_stdin)
	close(fd);				/* errors are ignored */
#ifdef HAVE_FD_CLOEXEC
    else
    {
	int fdflags = fcntl(fd, F_GETFD);
	if (fdflags >= 0 && (fdflags & FD_CLOEXEC) == 0)
	    (void)fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC);
    }
#endif
    vim_free(buffer);

#ifdef HAVE_DUP
    if (read_stdin)
    {
	/* Use stderr for stdin, makes shell commands work. */
	close(0);
	ignored = dup(2);
    }
#endif

#ifdef FEAT_MBYTE
    if (tmpname != NULL)
    {
	mch_remove(tmpname);		/* delete converted file */
	vim_free(tmpname);
    }
#endif
    --no_wait_return;			/* may wait for return now */

    /*
     * In recovery mode everything but autocommands is skipped.
     */
    if (!recoverymode)
    {
	/* need to delete the last line, which comes from the empty buffer */
	if (newfile && wasempty && !(curbuf->b_ml.ml_flags & ML_EMPTY))
	{
#ifdef FEAT_NETBEANS_INTG
	    netbeansFireChanges = 0;
#endif
	    ml_delete(curbuf->b_ml.ml_line_count, FALSE);
#ifdef FEAT_NETBEANS_INTG
	    netbeansFireChanges = 1;
#endif
	    --linecnt;
	}
	linecnt = curbuf->b_ml.ml_line_count - linecnt;
	if (filesize == 0)
	    linecnt = 0;
	if (newfile || read_buffer)
	{
	    redraw_curbuf_later(NOT_VALID);
#ifdef FEAT_DIFF
	    /* After reading the text into the buffer the diff info needs to
	     * be updated. */
	    diff_invalidate(curbuf);
#endif
#ifdef FEAT_FOLDING
	    /* All folds in the window are invalid now.  Mark them for update
	     * before triggering autocommands. */
	    foldUpdateAll(curwin);
#endif
	}
	else if (linecnt)		/* appended at least one line */
	    appended_lines_mark(from, linecnt);

#ifndef ALWAYS_USE_GUI
	/*
	 * If we were reading from the same terminal as where messages go,
	 * the screen will have been messed up.
	 * Switch on raw mode now and clear the screen.
	 */
	if (read_stdin)
	{
	    settmode(TMODE_RAW);	/* set to raw mode */
	    starttermcap();
	    screenclear();
	}
#endif

	if (got_int)
	{
	    if (!(flags & READ_DUMMY))
	    {
		filemess(curbuf, sfname, (char_u *)_(e_interr), 0);
		if (newfile)
		    curbuf->b_p_ro = TRUE;	/* must use "w!" now */
	    }
	    msg_scroll = msg_save;
#ifdef FEAT_VIMINFO
	    check_marks_read();
#endif
	    return OK;		/* an interrupt isn't really an error */
	}

	if (!filtering && !(flags & READ_DUMMY))
	{
	    msg_add_fname(curbuf, sfname);   /* fname in IObuff with quotes */
	    c = FALSE;

#ifdef UNIX
# ifdef S_ISFIFO
	    if (S_ISFIFO(perm))			    /* fifo or socket */
	    {
		STRCAT(IObuff, _("[fifo/socket]"));
		c = TRUE;
	    }
# else
#  ifdef S_IFIFO
	    if ((perm & S_IFMT) == S_IFIFO)	    /* fifo */
	    {
		STRCAT(IObuff, _("[fifo]"));
		c = TRUE;
	    }
#  endif
#  ifdef S_IFSOCK
	    if ((perm & S_IFMT) == S_IFSOCK)	    /* or socket */
	    {
		STRCAT(IObuff, _("[socket]"));
		c = TRUE;
	    }
#  endif
# endif
# ifdef OPEN_CHR_FILES
	    if (S_ISCHR(perm))			    /* or character special */
	    {
		STRCAT(IObuff, _("[character special]"));
		c = TRUE;
	    }
# endif
#endif
	    if (curbuf->b_p_ro)
	    {
		STRCAT(IObuff, shortmess(SHM_RO) ? _("[RO]") : _("[readonly]"));
		c = TRUE;
	    }
	    if (read_no_eol_lnum)
	    {
		msg_add_eol();
		c = TRUE;
	    }
	    if (ff_error == EOL_DOS)
	    {
		STRCAT(IObuff, _("[CR missing]"));
		c = TRUE;
	    }
	    if (split)
	    {
		STRCAT(IObuff, _("[long lines split]"));
		c = TRUE;
	    }
#ifdef FEAT_MBYTE
	    if (notconverted)
	    {
		STRCAT(IObuff, _("[NOT converted]"));
		c = TRUE;
	    }
	    else if (converted)
	    {
		STRCAT(IObuff, _("[converted]"));
		c = TRUE;
	    }
#endif
#ifdef FEAT_CRYPT
	    if (cryptkey != NULL)
	    {
		crypt_append_msg(curbuf);
		c = TRUE;
	    }
#endif
#ifdef FEAT_MBYTE
	    if (conv_error != 0)
	    {
		sprintf((char *)IObuff + STRLEN(IObuff),
		       _("[CONVERSION ERROR in line %ld]"), (long)conv_error);
		c = TRUE;
	    }
	    else if (illegal_byte > 0)
	    {
		sprintf((char *)IObuff + STRLEN(IObuff),
			 _("[ILLEGAL BYTE in line %ld]"), (long)illegal_byte);
		c = TRUE;
	    }
	    else
#endif
		if (error)
	    {
		STRCAT(IObuff, _("[READ ERRORS]"));
		c = TRUE;
	    }
	    if (msg_add_fileformat(fileformat))
		c = TRUE;
#ifdef FEAT_CRYPT
	    if (cryptkey != NULL)
		msg_add_lines(c, (long)linecnt, filesize
			 - crypt_get_header_len(crypt_get_method_nr(curbuf)));
	    else
#endif
		msg_add_lines(c, (long)linecnt, filesize);

	    vim_free(keep_msg);
	    keep_msg = NULL;
	    msg_scrolled_ign = TRUE;
#ifdef ALWAYS_USE_GUI
	    /* Don't show the message when reading stdin, it would end up in a
	     * message box (which might be shown when exiting!) */
	    if (read_stdin || read_buffer)
		p = msg_may_trunc(FALSE, IObuff);
	    else
#endif
		p = msg_trunc_attr(IObuff, FALSE, 0);
	    if (read_stdin || read_buffer || restart_edit != 0
		    || (msg_scrolled != 0 && !need_wait_return))
		/* Need to repeat the message after redrawing when:
		 * - When reading from stdin (the screen will be cleared next).
		 * - When restart_edit is set (otherwise there will be a delay
		 *   before redrawing).
		 * - When the screen was scrolled but there is no wait-return
		 *   prompt. */
		set_keep_msg(p, 0);
	    msg_scrolled_ign = FALSE;
	}

	/* with errors writing the file requires ":w!" */
	if (newfile && (error
#ifdef FEAT_MBYTE
		    || conv_error != 0
		    || (illegal_byte > 0 && bad_char_behavior != BAD_KEEP)
#endif
		    ))
	    curbuf->b_p_ro = TRUE;

	u_clearline();	    /* cannot use "U" command after adding lines */

	/*
	 * In Ex mode: cursor at last new line.
	 * Otherwise: cursor at first new line.
	 */
	if (exmode_active)
	    curwin->w_cursor.lnum = from + linecnt;
	else
	    curwin->w_cursor.lnum = from + 1;
	check_cursor_lnum();
	beginline(BL_WHITE | BL_FIX);	    /* on first non-blank */

	/*
	 * Set '[ and '] marks to the newly read lines.
	 */
	curbuf->b_op_start.lnum = from + 1;
	curbuf->b_op_start.col = 0;
	curbuf->b_op_end.lnum = from + linecnt;
	curbuf->b_op_end.col = 0;

#ifdef WIN32
	/*
	 * Work around a weird problem: When a file has two links (only
	 * possible on NTFS) and we write through one link, then stat() it
	 * through the other link, the timestamp information may be wrong.
	 * It's correct again after reading the file, thus reset the timestamp
	 * here.
	 */
	if (newfile && !read_stdin && !read_buffer
					 && mch_stat((char *)fname, &st) >= 0)
	{
	    buf_store_time(curbuf, &st, fname);
	    curbuf->b_mtime_read = curbuf->b_mtime;
	}
#endif
    }
    msg_scroll = msg_save;

#ifdef FEAT_VIMINFO
    /*
     * Get the marks before executing autocommands, so they can be used there.
     */
    check_marks_read();
#endif

    /*
     * We remember if the last line of the read didn't have
     * an eol even when 'binary' is off, to support turning 'fixeol' off,
     * or writing the read again with 'binary' on.  The latter is required
     * for ":autocmd FileReadPost *.gz set bin|'[,']!gunzip" to work.
     */
    curbuf->b_no_eol_lnum = read_no_eol_lnum;

    /* When reloading a buffer put the cursor at the first line that is
     * different. */
    if (flags & READ_KEEP_UNDO)
	u_find_first_changed();

#ifdef FEAT_PERSISTENT_UNDO
    /*
     * When opening a new file locate undo info and read it.
     */
    if (read_undo_file)
    {
	char_u	hash[UNDO_HASH_SIZE];

	sha256_finish(&sha_ctx, hash);
	u_read_undo(NULL, hash, fname);
    }
#endif

#ifdef FEAT_AUTOCMD
    if (!read_stdin && !read_fifo && (!read_buffer || sfname != NULL))
    {
	int m = msg_scroll;
	int n = msg_scrolled;

	/* Save the fileformat now, otherwise the buffer will be considered
	 * modified if the format/encoding was automatically detected. */
	if (set_options)
	    save_file_ff(curbuf);

	/*
	 * The output from the autocommands should not overwrite anything and
	 * should not be overwritten: Set msg_scroll, restore its value if no
	 * output was done.
	 */
	msg_scroll = TRUE;
	if (filtering)
	    apply_autocmds_exarg(EVENT_FILTERREADPOST, NULL, sfname,
							  FALSE, curbuf, eap);
	else if (newfile || (read_buffer && sfname != NULL))
	{
	    apply_autocmds_exarg(EVENT_BUFREADPOST, NULL, sfname,
							  FALSE, curbuf, eap);
	    if (!au_did_filetype && *curbuf->b_p_ft != NUL)
		/*
		 * EVENT_FILETYPE was not triggered but the buffer already has a
		 * filetype. Trigger EVENT_FILETYPE using the existing filetype.
		 */
		apply_autocmds(EVENT_FILETYPE, curbuf->b_p_ft, curbuf->b_fname,
			TRUE, curbuf);
	}
	else
	    apply_autocmds_exarg(EVENT_FILEREADPOST, sfname, sfname,
							    FALSE, NULL, eap);
	if (msg_scrolled == n)
	    msg_scroll = m;
# ifdef FEAT_EVAL
	if (aborting())	    /* autocmds may abort script processing */
	    return FAIL;
# endif
    }
#endif

    if (recoverymode && error)
	return FAIL;
    return OK;
}