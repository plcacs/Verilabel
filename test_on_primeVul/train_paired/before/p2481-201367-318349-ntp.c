save_config(
	struct recvbuf *rbufp,
	int restrict_mask
	)
{
#ifdef SAVECONFIG
	static const char savedconfig_eq[] = "savedconfig=";
	
	char filespec[128];
	char filename[128];
	char fullpath[512];
	char savedconfig[sizeof(savedconfig_eq) + sizeof(filename)];
	time_t now;
	int fd;
	FILE *fptr;
	int prc;
	size_t reqlen;
#endif

	if (RES_NOMODIFY & restrict_mask) {
		ctl_printf("%s", "saveconfig prohibited by restrict ... nomodify");
		ctl_flushpkt(0);
		NLOG(NLOG_SYSINFO)
			msyslog(LOG_NOTICE,
				"saveconfig from %s rejected due to nomodify restriction",
				stoa(&rbufp->recv_srcadr));
		sys_restricted++;
		return;
	}

#ifdef SAVECONFIG
	if (NULL == saveconfigdir) {
		ctl_printf("%s", "saveconfig prohibited, no saveconfigdir configured");
		ctl_flushpkt(0);
		NLOG(NLOG_SYSINFO)
			msyslog(LOG_NOTICE,
				"saveconfig from %s rejected, no saveconfigdir",
				stoa(&rbufp->recv_srcadr));
		return;
	}

	/* The length checking stuff gets serious. Do not assume a NUL
	 * byte can be found, but if so, use it to calculate the needed
	 * buffer size. If the available buffer is too short, bail out;
	 * likewise if there is no file spec. (The latter will not
	 * happen when using NTPQ, but there are other ways to craft a
	 * network packet!)
	 */
	reqlen = (size_t)(reqend - reqpt);
	if (0 != reqlen) {
		char * nulpos = (char*)memchr(reqpt, 0, reqlen);
		if (NULL != nulpos)
			reqlen = (size_t)(nulpos - reqpt);
	}
	if (0 == reqlen)
		return;
	if (reqlen >= sizeof(filespec)) {
		ctl_printf("saveconfig exceeded maximum raw name length (%u)",
			   (u_int)sizeof(filespec));
		ctl_flushpkt(0);
		msyslog(LOG_NOTICE,
			"saveconfig exceeded maximum raw name length from %s",
			stoa(&rbufp->recv_srcadr));
		return;
	}

	/* copy data directly as we exactly know the size */
	memcpy(filespec, reqpt, reqlen);
	filespec[reqlen] = '\0';
	
	/*
	 * allow timestamping of the saved config filename with
	 * strftime() format such as:
	 *   ntpq -c "saveconfig ntp-%Y%m%d-%H%M%S.conf"
	 * XXX: Nice feature, but not too safe.
	 * YYY: The check for permitted characters in file names should
	 *      weed out the worst. Let's hope 'strftime()' does not
	 *      develop pathological problems.
	 */
	time(&now);
	if (0 == strftime(filename, sizeof(filename), filespec,
			  localtime(&now)))
	{
		/*
		 * If we arrive here, 'strftime()' balked; most likely
		 * the buffer was too short. (Or it encounterd an empty
		 * format, or just a format that expands to an empty
		 * string.) We try to use the original name, though this
		 * is very likely to fail later if there are format
		 * specs in the string. Note that truncation cannot
		 * happen here as long as both buffers have the same
		 * size!
		 */
		strlcpy(filename, filespec, sizeof(filename));
	}

	/*
	 * Check the file name for sanity. This migth/will rule out file
	 * names that would be legal but problematic, and it blocks
	 * directory traversal.
	 */
	if (!is_safe_filename(filename)) {
		ctl_printf("saveconfig rejects unsafe file name '%s'",
			   filename);
		ctl_flushpkt(0);
		msyslog(LOG_NOTICE,
			"saveconfig rejects unsafe file name from %s",
			stoa(&rbufp->recv_srcadr));
		return;
	}

	/* concatenation of directory and path can cause another
	 * truncation...
	 */
	prc = snprintf(fullpath, sizeof(fullpath), "%s%s",
		       saveconfigdir, filename);
	if (prc < 0 || prc >= sizeof(fullpath)) {
		ctl_printf("saveconfig exceeded maximum path length (%u)",
			   (u_int)sizeof(fullpath));
		ctl_flushpkt(0);
		msyslog(LOG_NOTICE,
			"saveconfig exceeded maximum path length from %s",
			stoa(&rbufp->recv_srcadr));
		return;
	}

	fd = open(fullpath, O_CREAT | O_TRUNC | O_WRONLY,
		  S_IRUSR | S_IWUSR);
	if (-1 == fd)
		fptr = NULL;
	else
		fptr = fdopen(fd, "w");

	if (NULL == fptr || -1 == dump_all_config_trees(fptr, 1)) {
		ctl_printf("Unable to save configuration to file '%s'",
			   filename);
		msyslog(LOG_ERR,
			"saveconfig %s from %s failed", filename,
			stoa(&rbufp->recv_srcadr));
	} else {
		ctl_printf("Configuration saved to '%s'", filename);
		msyslog(LOG_NOTICE,
			"Configuration saved to '%s' (requested by %s)",
			fullpath, stoa(&rbufp->recv_srcadr));
		/*
		 * save the output filename in system variable
		 * savedconfig, retrieved with:
		 *   ntpq -c "rv 0 savedconfig"
		 * Note: the way 'savedconfig' is defined makes overflow
		 * checks unnecessary here.
		 */
		snprintf(savedconfig, sizeof(savedconfig), "%s%s",
			 savedconfig_eq, filename);
		set_sys_var(savedconfig, strlen(savedconfig) + 1, RO);
	}

	if (NULL != fptr)
		fclose(fptr);
#else	/* !SAVECONFIG follows */
	ctl_printf("%s",
		   "saveconfig unavailable, configured with --disable-saveconfig");
#endif	
	ctl_flushpkt(0);
}