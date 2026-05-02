parseURL(char *url, ParsedURL *p_url, ParsedURL *current)
{
    char *p, *q, *qq;
    Str tmp;

    url = url_quote(url);	/* quote 0x01-0x20, 0x7F-0xFF */

    p = url;
    copyParsedURL(p_url, NULL);
    p_url->scheme = SCM_MISSING;

    /* RFC1808: Relative Uniform Resource Locators
     * 4.  Resolving Relative URLs
     */
    if (*url == '\0' || *url == '#') {
	if (current)
	    copyParsedURL(p_url, current);
	goto do_label;
    }
#if defined( __EMX__ ) || defined( __CYGWIN__ )
    if (!strncasecmp(url, "file://localhost/", 17)) {
	p_url->scheme = SCM_LOCAL;
	p += 17 - 1;
	url += 17 - 1;
    }
#endif
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (IS_ALPHA(*p) && (p[1] == ':' || p[1] == '|')) {
	p_url->scheme = SCM_LOCAL;
	goto analyze_file;
    }
#endif				/* SUPPORT_DOS_DRIVE_PREFIX */
    /* search for scheme */
    p_url->scheme = getURLScheme(&p);
    if (p_url->scheme == SCM_MISSING) {
	/* scheme part is not found in the url. This means either
	 * (a) the url is relative to the current or (b) the url
	 * denotes a filename (therefore the scheme is SCM_LOCAL).
	 */
	if (current) {
	    switch (current->scheme) {
	    case SCM_LOCAL:
	    case SCM_LOCAL_CGI:
		p_url->scheme = SCM_LOCAL;
		break;
	    case SCM_FTP:
	    case SCM_FTPDIR:
		p_url->scheme = SCM_FTP;
		break;
#ifdef USE_NNTP
	    case SCM_NNTP:
	    case SCM_NNTP_GROUP:
		p_url->scheme = SCM_NNTP;
		break;
	    case SCM_NEWS:
	    case SCM_NEWS_GROUP:
		p_url->scheme = SCM_NEWS;
		break;
#endif
	    default:
		p_url->scheme = current->scheme;
		break;
	    }
	}
	else
	    p_url->scheme = SCM_LOCAL;
	p = url;
	if (!strncmp(p, "//", 2)) {
	    /* URL begins with // */
	    /* it means that 'scheme:' is abbreviated */
	    p += 2;
	    goto analyze_url;
	}
	/* the url doesn't begin with '//' */
	goto analyze_file;
    }
    /* scheme part has been found */
    if (p_url->scheme == SCM_UNKNOWN) {
	p_url->file = allocStr(url, -1);
	return;
    }
    /* get host and port */
    if (p[0] != '/' || p[1] != '/') {	/* scheme:foo or scheme:/foo */
	p_url->host = NULL;
	if (p_url->scheme != SCM_UNKNOWN)
	    p_url->port = DefaultPort[p_url->scheme];
	else
	    p_url->port = 0;
	goto analyze_file;
    }
    /* after here, p begins with // */
    if (p_url->scheme == SCM_LOCAL) {	/* file://foo           */
#ifdef __EMX__
	p += 2;
	goto analyze_file;
#else
	if (p[2] == '/' || p[2] == '~'
	    /* <A HREF="file:///foo">file:///foo</A>  or <A HREF="file://~user">file://~user</A> */
#ifdef SUPPORT_DOS_DRIVE_PREFIX
	    || (IS_ALPHA(p[2]) && (p[3] == ':' || p[3] == '|'))
	    /* <A HREF="file://DRIVE/foo">file://DRIVE/foo</A> */
#endif				/* SUPPORT_DOS_DRIVE_PREFIX */
	    ) {
	    p += 2;
	    goto analyze_file;
	}
#endif				/* __EMX__ */
    }
    p += 2;			/* scheme://foo         */
    /*          ^p is here  */
  analyze_url:
    q = p;
#ifdef INET6
    if (*q == '[') {		/* rfc2732,rfc2373 compliance */
	p++;
	while (IS_XDIGIT(*p) || *p == ':' || *p == '.')
	    p++;
	if (*p != ']' || (*(p + 1) && strchr(":/?#", *(p + 1)) == NULL))
	    p = q;
    }
#endif
    while (*p && strchr(":/@?#", *p) == NULL)
	p++;
    switch (*p) {
    case ':':
	/* scheme://user:pass@host or
	 * scheme://host:port
	 */
	qq = q;
	q = ++p;
	while (*p && strchr("@/?#", *p) == NULL)
	    p++;
	if (*p == '@') {
	    /* scheme://user:pass@...       */
	    p_url->user = copyPath(qq, q - 1 - qq, COPYPATH_SPC_IGNORE);
	    p_url->pass = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
	    p++;
	    goto analyze_url;
	}
	/* scheme://host:port/ */
	p_url->host = copyPath(qq, q - 1 - qq,
			       COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
	tmp = Strnew_charp_n(q, p - q);
	p_url->port = atoi(tmp->ptr);
	/* *p is one of ['\0', '/', '?', '#'] */
	break;
    case '@':
	/* scheme://user@...            */
	p_url->user = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
	p++;
	goto analyze_url;
    case '\0':
	/* scheme://host                */
    case '/':
    case '?':
    case '#':
	p_url->host = copyPath(q, p - q,
			       COPYPATH_SPC_IGNORE | COPYPATH_LOWERCASE);
	p_url->port = DefaultPort[p_url->scheme];
	break;
    }
  analyze_file:
#ifndef SUPPORT_NETBIOS_SHARE
    if (p_url->scheme == SCM_LOCAL && p_url->user == NULL &&
	p_url->host != NULL && *p_url->host != '\0' &&
	strcmp(p_url->host, "localhost")) {
	/*
	 * In the environments other than CYGWIN, a URL like 
	 * file://host/file is regarded as ftp://host/file.
	 * On the other hand, file://host/file on CYGWIN is
	 * regarded as local access to the file //host/file.
	 * `host' is a netbios-hostname, drive, or any other
	 * name; It is CYGWIN system call who interprets that.
	 */

	p_url->scheme = SCM_FTP;	/* ftp://host/... */
	if (p_url->port == 0)
	    p_url->port = DefaultPort[SCM_FTP];
    }
#endif
    if ((*p == '\0' || *p == '#' || *p == '?') && p_url->host == NULL) {
	p_url->file = "";
	goto do_query;
    }
#ifdef SUPPORT_DOS_DRIVE_PREFIX
    if (p_url->scheme == SCM_LOCAL) {
	q = p;
	if (*q == '/')
	    q++;
	if (IS_ALPHA(q[0]) && (q[1] == ':' || q[1] == '|')) {
	    if (q[1] == '|') {
		p = allocStr(q, -1);
		p[1] = ':';
	    }
	    else
		p = q;
	}
    }
#endif

    q = p;
#ifdef USE_GOPHER
    if (p_url->scheme == SCM_GOPHER) {
	if (*q == '/')
	    q++;
	if (*q && q[0] != '/' && q[1] != '/' && q[2] == '/')
	    q++;
    }
#endif				/* USE_GOPHER */
    if (*p == '/')
	p++;
    if (*p == '\0' || *p == '#' || *p == '?') {	/* scheme://host[:port]/ */
	p_url->file = DefaultFile(p_url->scheme);
	goto do_query;
    }
#ifdef USE_GOPHER
    if (p_url->scheme == SCM_GOPHER && *p == 'R') {
	p++;
	tmp = Strnew();
	Strcat_char(tmp, *(p++));
	while (*p && *p != '/')
	    p++;
	Strcat_charp(tmp, p);
	while (*p)
	    p++;
	p_url->file = copyPath(tmp->ptr, -1, COPYPATH_SPC_IGNORE);
    }
    else
#endif				/* USE_GOPHER */
    {
	char *cgi = strchr(p, '?');
      again:
	while (*p && *p != '#' && p != cgi)
	    p++;
	if (*p == '#' && p_url->scheme == SCM_LOCAL) {
	    /* 
	     * According to RFC2396, # means the beginning of
	     * URI-reference, and # should be escaped.  But,
	     * if the scheme is SCM_LOCAL, the special
	     * treatment will apply to # for convinience.
	     */
	    if (p > q && *(p - 1) == '/' && (cgi == NULL || p < cgi)) {
		/* 
		 * # comes as the first character of the file name
		 * that means, # is not a label but a part of the file
		 * name.
		 */
		p++;
		goto again;
	    }
	    else if (*(p + 1) == '\0') {
		/* 
		 * # comes as the last character of the file name that
		 * means, # is not a label but a part of the file
		 * name.
		 */
		p++;
	    }
	}
	if (p_url->scheme == SCM_LOCAL || p_url->scheme == SCM_MISSING)
	    p_url->file = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
	else
	    p_url->file = copyPath(q, p - q, COPYPATH_SPC_IGNORE);
    }

  do_query:
    if (*p == '?') {
	q = ++p;
	while (*p && *p != '#')
	    p++;
	p_url->query = copyPath(q, p - q, COPYPATH_SPC_ALLOW);
    }
  do_label:
    if (p_url->scheme == SCM_MISSING) {
	p_url->scheme = SCM_LOCAL;
	p_url->file = allocStr(p, -1);
	p_url->label = NULL;
    }
    else if (*p == '#')
	p_url->label = allocStr(p + 1, -1);
    else
	p_url->label = NULL;
}