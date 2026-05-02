save_config(
	struct recvbuf *rbufp,
	int restrict_mask
	)
{
	char reply[128];
#ifdef SAVECONFIG
	char filespec[128];
	char filename[128];
	char fullpath[512];
	const char savedconfig_eq[] = "savedconfig=";
	char savedconfig[sizeof(savedconfig_eq) + sizeof(filename)];
	time_t now;
	int fd;
	FILE *fptr;
#endif

	if (RES_NOMODIFY & restrict_mask) {
		snprintf(reply, sizeof(reply),
			 "saveconfig prohibited by restrict ... nomodify");
		ctl_putdata(reply, strlen(reply), 0);
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
		snprintf(reply, sizeof(reply),
			 "saveconfig prohibited, no saveconfigdir configured");
		ctl_putdata(reply, strlen(reply), 0);
		ctl_flushpkt(0);
		NLOG(NLOG_SYSINFO)
			msyslog(LOG_NOTICE,
				"saveconfig from %s rejected, no saveconfigdir",
				stoa(&rbufp->recv_srcadr));
		return;
	}

	if (0 == reqend - reqpt)
		return;

	strlcpy(filespec, reqpt, sizeof(filespec));
	time(&now);

	/*
	 * allow timestamping of the saved config filename with
	 * strftime() format such as:
	 *   ntpq -c "saveconfig ntp-%Y%m%d-%H%M%S.conf"
	 * XXX: Nice feature, but not too safe.
	 */
	if (0 == strftime(filename, sizeof(filename), filespec,
			       localtime(&now)))
		strlcpy(filename, filespec, sizeof(filename));

	/*
	 * Conceptually we should be searching for DIRSEP in filename,
	 * however Windows actually recognizes both forward and
	 * backslashes as equivalent directory separators at the API
	 * level.  On POSIX systems we could allow '\\' but such
	 * filenames are tricky to manipulate from a shell, so just
	 * reject both types of slashes on all platforms.
	 */
	if (strchr(filename, '\\') || strchr(filename, '/')) {
		snprintf(reply, sizeof(reply),
			 "saveconfig does not allow directory in filename");
		ctl_putdata(reply, strlen(reply), 0);
		ctl_flushpkt(0);
		msyslog(LOG_NOTICE,
			"saveconfig with path from %s rejected",
			stoa(&rbufp->recv_srcadr));
		return;
	}

	snprintf(fullpath, sizeof(fullpath), "%s%s",
		 saveconfigdir, filename);

	fd = open(fullpath, O_CREAT | O_TRUNC | O_WRONLY,
		  S_IRUSR | S_IWUSR);
	if (-1 == fd)
		fptr = NULL;
	else
		fptr = fdopen(fd, "w");

	if (NULL == fptr || -1 == dump_all_config_trees(fptr, 1)) {
		snprintf(reply, sizeof(reply),
			 "Unable to save configuration to file %s",
			 filename);
		msyslog(LOG_ERR,
			"saveconfig %s from %s failed", filename,
			stoa(&rbufp->recv_srcadr));
	} else {
		snprintf(reply, sizeof(reply),
			 "Configuration saved to %s", filename);
		msyslog(LOG_NOTICE,
			"Configuration saved to %s (requested by %s)",
			fullpath, stoa(&rbufp->recv_srcadr));
		/*
		 * save the output filename in system variable
		 * savedconfig, retrieved with:
		 *   ntpq -c "rv 0 savedconfig"
		 */
		snprintf(savedconfig, sizeof(savedconfig), "%s%s",
			 savedconfig_eq, filename);
		set_sys_var(savedconfig, strlen(savedconfig) + 1, RO);
	}

	if (NULL != fptr)
		fclose(fptr);
#else	/* !SAVECONFIG follows */
	snprintf(reply, sizeof(reply),
		 "saveconfig unavailable, configured with --disable-saveconfig");
#endif

	ctl_putdata(reply, strlen(reply), 0);
	ctl_flushpkt(0);
}