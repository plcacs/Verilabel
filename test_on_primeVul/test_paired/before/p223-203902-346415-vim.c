get_one_sourceline(source_cookie_T *sp)
{
    garray_T		ga;
    int			len;
    int			c;
    char_u		*buf;
#ifdef USE_CRNL
    int			has_cr;		// CR-LF found
#endif
    int			have_read = FALSE;

    // use a growarray to store the sourced line
    ga_init2(&ga, 1, 250);

    // Loop until there is a finished line (or end-of-file).
    ++sp->sourcing_lnum;
    for (;;)
    {
	// make room to read at least 120 (more) characters
	if (ga_grow(&ga, 120) == FAIL)
	    break;
	if (sp->source_from_buf)
	{
	    if (sp->buf_lnum >= sp->buflines.ga_len)
		break;		    // all the lines are processed
	    ga_concat(&ga, ((char_u **)sp->buflines.ga_data)[sp->buf_lnum]);
	    sp->buf_lnum++;
	    if (ga_grow(&ga, 1) == FAIL)
		break;
	    buf = (char_u *)ga.ga_data;
	    buf[ga.ga_len++] = NUL;
	}
	else
	{
	    buf = (char_u *)ga.ga_data;
	    if (fgets((char *)buf + ga.ga_len, ga.ga_maxlen - ga.ga_len,
			sp->fp) == NULL)
		break;
	}
	len = ga.ga_len + (int)STRLEN(buf + ga.ga_len);
#ifdef USE_CRNL
	// Ignore a trailing CTRL-Z, when in Dos mode.	Only recognize the
	// CTRL-Z by its own, or after a NL.
	if (	   (len == 1 || (len >= 2 && buf[len - 2] == '\n'))
		&& sp->fileformat == EOL_DOS
		&& buf[len - 1] == Ctrl_Z)
	{
	    buf[len - 1] = NUL;
	    break;
	}
#endif

	have_read = TRUE;
	ga.ga_len = len;

	// If the line was longer than the buffer, read more.
	if (ga.ga_maxlen - ga.ga_len == 1 && buf[len - 1] != '\n')
	    continue;

	if (len >= 1 && buf[len - 1] == '\n')	// remove trailing NL
	{
#ifdef USE_CRNL
	    has_cr = (len >= 2 && buf[len - 2] == '\r');
	    if (sp->fileformat == EOL_UNKNOWN)
	    {
		if (has_cr)
		    sp->fileformat = EOL_DOS;
		else
		    sp->fileformat = EOL_UNIX;
	    }

	    if (sp->fileformat == EOL_DOS)
	    {
		if (has_cr)	    // replace trailing CR
		{
		    buf[len - 2] = '\n';
		    --len;
		    --ga.ga_len;
		}
		else	    // lines like ":map xx yy^M" will have failed
		{
		    if (!sp->error)
		    {
			msg_source(HL_ATTR(HLF_W));
			emsg(_("W15: Warning: Wrong line separator, ^M may be missing"));
		    }
		    sp->error = TRUE;
		    sp->fileformat = EOL_UNIX;
		}
	    }
#endif
	    // The '\n' is escaped if there is an odd number of ^V's just
	    // before it, first set "c" just before the 'V's and then check
	    // len&c parities (is faster than ((len-c)%2 == 0)) -- Acevedo
	    for (c = len - 2; c >= 0 && buf[c] == Ctrl_V; c--)
		;
	    if ((len & 1) != (c & 1))	// escaped NL, read more
	    {
		++sp->sourcing_lnum;
		continue;
	    }

	    buf[len - 1] = NUL;		// remove the NL
	}

	// Check for ^C here now and then, so recursive :so can be broken.
	line_breakcheck();
	break;
    }

    if (have_read)
	return (char_u *)ga.ga_data;

    vim_free(ga.ga_data);
    return NULL;
}