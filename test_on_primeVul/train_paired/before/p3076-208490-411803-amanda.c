main(
    int		argc,
    char **	argv)
{
#ifdef GNUTAR
    int i;
    char *e;
    char *dbf;
    char *cmdline;
    GPtrArray *array = g_ptr_array_new();
    gchar **strings;
    char **new_argv;
    char **env;
#endif

    if (argc > 1 && argv[1] && g_str_equal(argv[1], "--version")) {
	printf("runtar-%s\n", VERSION);
	return (0);
    }

    /*
     * Configure program for internationalization:
     *   1) Only set the message locale for now.
     *   2) Set textdomain for all amanda related programs to "amanda"
     *      We don't want to be forced to support dozens of message catalogs.
     */
    setlocale(LC_MESSAGES, "C");
    textdomain("amanda"); 

    safe_fd(-1, 0);
    safe_cd();

    set_pname("runtar");

    /* Don't die when child closes pipe */
    signal(SIGPIPE, SIG_IGN);

    dbopen(DBG_SUBDIR_CLIENT);
    config_init(CONFIG_INIT_CLIENT|CONFIG_INIT_GLOBAL, NULL);

    if (argc < 3) {
	error(_("Need at least 3 arguments\n"));
	/*NOTREACHED*/
    }

    dbprintf(_("version %s\n"), VERSION);

    if (!g_str_equal(argv[3], "--create")) {
	error(_("Can only be used to create tar archives\n"));
	/*NOTREACHED*/
    }

#ifndef GNUTAR

    g_fprintf(stderr,_("gnutar not available on this system.\n"));
    dbprintf(_("%s: gnutar not available on this system.\n"), argv[0]);
    dbclose();
    return 1;

#else

    /*
     * Print out version information for tar.
     */
    do {
	FILE *	version_file;
	char	version_buf[80];

	if ((version_file = popen(GNUTAR " --version 2>&1", "r")) != NULL) {
	    if (fgets(version_buf, (int)sizeof(version_buf), version_file) != NULL) {
		dbprintf(_(GNUTAR " version: %s\n"), version_buf);
	    } else {
		if (ferror(version_file)) {
		    dbprintf(_(GNUTAR " version: Read failure: %s\n"), strerror(errno));
		} else {
		    dbprintf(_(GNUTAR " version: Read failure; EOF\n"));
		}
	    }
	} else {
	    dbprintf(_(GNUTAR " version: unavailable: %s\n"), strerror(errno));
	}
    } while(0);

#ifdef WANT_SETUID_CLIENT
    check_running_as(RUNNING_AS_CLIENT_LOGIN | RUNNING_AS_UID_ONLY);
    if (!become_root()) {
	error(_("error [%s could not become root (is the setuid bit set?)]\n"), get_pname());
	/*NOTREACHED*/
    }
#else
    check_running_as(RUNNING_AS_CLIENT_LOGIN);
#endif

    /* skip argv[0] */
    argc--;
    argv++;

    dbprintf(_("config: %s\n"), argv[0]);
    if (!g_str_equal(argv[0], "NOCONFIG"))
	dbrename(argv[0], DBG_SUBDIR_CLIENT);
    argc--;
    argv++;

    new_argv = g_new0(char *, argc+1);

    new_argv[0] = g_strdup_printf("%s", argv[0]);
    g_ptr_array_add(array, g_strdup(GNUTAR));
    for (i = 1; argv[i]; i++) {
        g_ptr_array_add(array, quote_string(argv[i]));
	new_argv[i] = g_strdup_printf("%s", argv[i]);
    }

    g_ptr_array_add(array, NULL);
    strings = (gchar **)g_ptr_array_free(array, FALSE);

    cmdline = g_strjoinv(" ", strings);
    g_strfreev(strings);

    dbprintf(_("running: %s\n"), cmdline);
    amfree(cmdline);

    dbf = dbfn();
    if (dbf) {
	dbf = g_strdup(dbf);
    }
    dbclose();

    env = safe_env();
    execve(GNUTAR, new_argv, env);
    free_env(env);

    e = strerror(errno);
    dbreopen(dbf, "more");
    amfree(dbf);
    dbprintf(_("execve of %s failed (%s)\n"), GNUTAR, e);
    dbclose();

    g_fprintf(stderr, _("runtar: could not exec %s: %s\n"), GNUTAR, e);
    return 1;
#endif
}