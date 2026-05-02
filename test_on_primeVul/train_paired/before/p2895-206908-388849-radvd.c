main(int argc, char *argv[])
{
	char pidstr[16];
	ssize_t ret;
	int c, log_method;
	char *logfile, *pidfile;
	int facility, fd;
	char *username = NULL;
	char *chrootdir = NULL;
	int configtest = 0;
	int singleprocess = 0;
#ifdef HAVE_GETOPT_LONG
	int opt_idx;
#endif

	pname = ((pname=strrchr(argv[0],'/')) != NULL)?pname+1:argv[0];

	srand((unsigned int)time(NULL));

	log_method = L_STDERR_SYSLOG;
	logfile = PATH_RADVD_LOG;
	conf_file = PATH_RADVD_CONF;
	facility = LOG_FACILITY;
	pidfile = PATH_RADVD_PID;

	/* parse args */
#define OPTIONS_STR "d:C:l:m:p:t:u:vhcs"
#ifdef HAVE_GETOPT_LONG
	while ((c = getopt_long(argc, argv, OPTIONS_STR, prog_opt, &opt_idx)) > 0)
#else
	while ((c = getopt(argc, argv, OPTIONS_STR)) > 0)
#endif
	{
		switch (c) {
		case 'C':
			conf_file = optarg;
			break;
		case 'd':
			set_debuglevel(atoi(optarg));
			break;
		case 'f':
			facility = atoi(optarg);
			break;
		case 'l':
			logfile = optarg;
			break;
		case 'p':
			pidfile = optarg;
			break;
		case 'm':
			if (!strcmp(optarg, "syslog"))
			{
				log_method = L_SYSLOG;
			}
			else if (!strcmp(optarg, "stderr_syslog"))
			{
				log_method = L_STDERR_SYSLOG;
			}
			else if (!strcmp(optarg, "stderr"))
			{
				log_method = L_STDERR;
			}
			else if (!strcmp(optarg, "logfile"))
			{
				log_method = L_LOGFILE;
			}
			else if (!strcmp(optarg, "none"))
			{
				log_method = L_NONE;
			}
			else
			{
				fprintf(stderr, "%s: unknown log method: %s\n", pname, optarg);
				exit(1);
			}
			break;
		case 't':
			chrootdir = strdup(optarg);
			break;
		case 'u':
			username = strdup(optarg);
			break;
		case 'v':
			version();
			break;
		case 'c':
			configtest = 1;
			break;
		case 's':
			singleprocess = 1;
			break;
		case 'h':
			usage();
#ifdef HAVE_GETOPT_LONG
		case ':':
			fprintf(stderr, "%s: option %s: parameter expected\n", pname,
				prog_opt[opt_idx].name);
			exit(1);
#endif
		case '?':
			exit(1);
		}
	}

	if (chrootdir) {
		if (!username) {
			fprintf(stderr, "Chroot as root is not safe, exiting\n");
			exit(1);
		}

		if (chroot(chrootdir) == -1) {
			perror("chroot");
			exit (1);
		}

		if (chdir("/") == -1) {
			perror("chdir");
			exit (1);
		}
		/* username will be switched later */
	}

	if (configtest) {
		log_method = L_STDERR;
	}

	if (log_open(log_method, pname, logfile, facility) < 0) {
		perror("log_open");
		exit(1);
	}

	if (!configtest) {
		flog(LOG_INFO, "version %s started", VERSION);
	}

	/* get a raw socket for sending and receiving ICMPv6 messages */
	sock = open_icmpv6_socket();
	if (sock < 0) {
		perror("open_icmpv6_socket");
		exit(1);
	}

	/* check that 'other' cannot write the file
         * for non-root, also that self/own group can't either
         */
	if (check_conffile_perm(username, conf_file) < 0) {
		if (get_debuglevel() == 0) {
			flog(LOG_ERR, "Exiting, permissions on conf_file invalid.\n");
			exit(1);
		}
		else
			flog(LOG_WARNING, "Insecure file permissions, but continuing anyway");
	}

	/* if we know how to do it, check whether forwarding is enabled */
	if (check_ip6_forwarding()) {
		flog(LOG_WARNING, "IPv6 forwarding seems to be disabled, but continuing anyway.");
	}

	/* parse config file */
	if (readin_config(conf_file) < 0) {
		flog(LOG_ERR, "Exiting, failed to read config file.\n");
		exit(1);
	}

	if (configtest) {
		fprintf(stderr, "Syntax OK\n");
		exit(0);
	}

	/* drop root privileges if requested. */
	if (username) {
		if (!singleprocess) {
		 	dlog(LOG_DEBUG, 3, "Initializing privsep");
		 	if (privsep_init() < 0)
				perror("Failed to initialize privsep.");
		}

		if (drop_root_privileges(username) < 0) {
			perror("drop_root_privileges");
			exit(1);
		}
	}

	if ((fd = open(pidfile, O_RDONLY, 0)) > 0)
	{
		ret = read(fd, pidstr, sizeof(pidstr) - 1);
		if (ret < 0)
		{
			flog(LOG_ERR, "cannot read radvd pid file, terminating: %s", strerror(errno));
			exit(1);
		}
		pidstr[ret] = '\0';
		if (!kill((pid_t)atol(pidstr), 0))
		{
			flog(LOG_ERR, "radvd already running, terminating.");
			exit(1);
		}
		close(fd);
		fd = open(pidfile, O_CREAT|O_TRUNC|O_WRONLY, 0644);
	}
	else	/* FIXME: not atomic if pidfile is on an NFS mounted volume */
		fd = open(pidfile, O_CREAT|O_EXCL|O_WRONLY, 0644);

	if (fd < 0)
	{
		flog(LOG_ERR, "cannot create radvd pid file, terminating: %s", strerror(errno));
		exit(1);
	}

	/*
	 * okay, config file is read in, socket and stuff is setup, so
	 * lets fork now...
	 */

	if (get_debuglevel() == 0) {

		/* Detach from controlling terminal */
		if (daemon(0, 0) < 0)
			perror("daemon");

		/* close old logfiles, including stderr */
		log_close();

		/* reopen logfiles, but don't log to stderr unless explicitly requested */
		if (log_method == L_STDERR_SYSLOG)
			log_method = L_SYSLOG;
		if (log_open(log_method, pname, logfile, facility) < 0) {
			perror("log_open");
			exit(1);
		}

	}

	/*
	 *	config signal handlers
	 */
	signal(SIGHUP, sighup_handler);
	signal(SIGTERM, sigterm_handler);
	signal(SIGINT, sigint_handler);
	signal(SIGUSR1, sigusr1_handler);

	snprintf(pidstr, sizeof(pidstr), "%ld\n", (long)getpid());

	ret = write(fd, pidstr, strlen(pidstr));
	if (ret != strlen(pidstr))
	{
		flog(LOG_ERR, "cannot write radvd pid file, terminating: %s", strerror(errno));
		exit(1);
	}

	close(fd);

	config_interface();
	kickoff_adverts();
	main_loop();
	stop_adverts();
	unlink(pidfile);

	return 0;
}