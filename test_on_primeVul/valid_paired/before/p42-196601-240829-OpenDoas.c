main(int argc, char **argv)
{
	const char *safepath = "/bin:/sbin:/usr/bin:/usr/sbin:"
	    "/usr/local/bin:/usr/local/sbin";
	const char *confpath = NULL;
	char *shargv[] = { NULL, NULL };
	char *sh;
	const char *p;
	const char *cmd;
	char cmdline[LINE_MAX];
	struct passwd mypwstore, targpwstore;
	struct passwd *mypw, *targpw;
	const struct rule *rule;
	uid_t uid;
	uid_t target = 0;
	gid_t groups[NGROUPS_MAX + 1];
	int ngroups;
	int i, ch, rv;
	int sflag = 0;
	int nflag = 0;
	char cwdpath[PATH_MAX];
	const char *cwd;
	char **envp;

	setprogname("doas");

	closefrom(STDERR_FILENO + 1);

	uid = getuid();

	while ((ch = getopt(argc, argv, "+C:Lnsu:")) != -1) {
		switch (ch) {
		case 'C':
			confpath = optarg;
			break;
		case 'L':
#if defined(USE_TIMESTAMP)
			exit(timestamp_clear() == -1);
#else
			exit(0);
#endif
		case 'u':
			if (parseuid(optarg, &target) != 0)
				errx(1, "unknown user");
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (confpath) {
		if (sflag)
			usage();
	} else if ((!sflag && !argc) || (sflag && argc))
		usage();

	rv = mygetpwuid_r(uid, &mypwstore, &mypw);
	if (rv != 0)
		err(1, "getpwuid_r failed");
	if (mypw == NULL)
		errx(1, "no passwd entry for self");
	ngroups = getgroups(NGROUPS_MAX, groups);
	if (ngroups == -1)
		err(1, "can't get groups");
	groups[ngroups++] = getgid();

	if (sflag) {
		sh = getenv("SHELL");
		if (sh == NULL || *sh == '\0') {
			shargv[0] = mypw->pw_shell;
		} else
			shargv[0] = sh;
		argv = shargv;
		argc = 1;
	}

	if (confpath) {
		checkconfig(confpath, argc, argv, uid, groups, ngroups,
		    target);
		exit(1);	/* fail safe */
	}

	if (geteuid())
		errx(1, "not installed setuid");

	parseconfig(DOAS_CONF, 1);

	/* cmdline is used only for logging, no need to abort on truncate */
	(void)strlcpy(cmdline, argv[0], sizeof(cmdline));
	for (i = 1; i < argc; i++) {
		if (strlcat(cmdline, " ", sizeof(cmdline)) >= sizeof(cmdline))
			break;
		if (strlcat(cmdline, argv[i], sizeof(cmdline)) >= sizeof(cmdline))
			break;
	}

	cmd = argv[0];
	if (!permit(uid, groups, ngroups, &rule, target, cmd,
	    (const char **)argv + 1)) {
		syslog(LOG_AUTHPRIV | LOG_NOTICE,
		    "command not permitted for %s: %s", mypw->pw_name, cmdline);
		errc(1, EPERM, NULL);
	}

#if defined(USE_SHADOW)
	if (!(rule->options & NOPASS)) {
		if (nflag)
			errx(1, "Authorization required");

		shadowauth(mypw->pw_name, rule->options & PERSIST);
	}
#elif !defined(USE_PAM)
	/* no authentication provider, only allow NOPASS rules */
	(void) nflag;
	if (!(rule->options & NOPASS))
		errx(1, "Authorization required");
#endif

	if ((p = getenv("PATH")) != NULL)
		formerpath = strdup(p);
	if (formerpath == NULL)
		formerpath = "";

	if (rule->cmd) {
		if (setenv("PATH", safepath, 1) == -1)
			err(1, "failed to set PATH '%s'", safepath);
	}

	rv = mygetpwuid_r(target, &targpwstore, &targpw);
	if (rv != 0)
		err(1, "getpwuid_r failed");
	if (targpw == NULL)
		errx(1, "no passwd entry for target");

#if defined(USE_PAM)
	pamauth(targpw->pw_name, mypw->pw_name, !nflag, rule->options & NOPASS,
	    rule->options & PERSIST);
#endif

#ifdef HAVE_LOGIN_CAP_H
	if (setusercontext(NULL, targpw, target, LOGIN_SETGROUP |
	    LOGIN_SETPRIORITY | LOGIN_SETRESOURCES | LOGIN_SETUMASK |
	    LOGIN_SETUSER) != 0)
		errx(1, "failed to set user context for target");
#else
	if (setresgid(targpw->pw_gid, targpw->pw_gid, targpw->pw_gid) != 0)
		err(1, "setresgid");
	if (initgroups(targpw->pw_name, targpw->pw_gid) != 0)
		err(1, "initgroups");
	if (setresuid(target, target, target) != 0)
		err(1, "setresuid");
#endif

	if (getcwd(cwdpath, sizeof(cwdpath)) == NULL)
		cwd = "(failed)";
	else
		cwd = cwdpath;

	if (!(rule->options & NOLOG)) {
		syslog(LOG_AUTHPRIV | LOG_INFO,
		    "%s ran command %s as %s from %s",
		    mypw->pw_name, cmdline, targpw->pw_name, cwd);
	}

	envp = prepenv(rule, mypw, targpw);

	/* setusercontext set path for the next process, so reset it for us */
	if (rule->cmd) {
		if (setenv("PATH", safepath, 1) == -1)
			err(1, "failed to set PATH '%s'", safepath);
	} else {
		if (setenv("PATH", formerpath, 1) == -1)
			err(1, "failed to set PATH '%s'", formerpath);
	}
	execvpe(cmd, argv, envp);
	if (errno == ENOENT)
		errx(1, "%s: command not found", cmd);
	err(1, "%s", cmd);
}