static int spawn_https_helper_openssl(const char *host, unsigned port)
{
	char *allocated = NULL;
	char *servername;
	int sp[2];
	int pid;
	IF_FEATURE_WGET_HTTPS(volatile int child_failed = 0;)

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) != 0)
		/* Kernel can have AF_UNIX support disabled */
		bb_simple_perror_msg_and_die("socketpair");

	if (!strchr(host, ':'))
		host = allocated = xasprintf("%s:%u", host, port);
	servername = xstrdup(host);
	strrchr(servername, ':')[0] = '\0';

	fflush_all();
	pid = xvfork();
	if (pid == 0) {
		/* Child */
		char *argv[8];

		close(sp[0]);
		xmove_fd(sp[1], 0);
		xdup2(0, 1);
		/*
		 * openssl s_client -quiet -connect www.kernel.org:443 2>/dev/null
		 * It prints some debug stuff on stderr, don't know how to suppress it.
		 * Work around by dev-nulling stderr. We lose all error messages :(
		 */
		xmove_fd(2, 3);
		xopen("/dev/null", O_RDWR);
		memset(&argv, 0, sizeof(argv));
		argv[0] = (char*)"openssl";
		argv[1] = (char*)"s_client";
		argv[2] = (char*)"-quiet";
		argv[3] = (char*)"-connect";
		argv[4] = (char*)host;
		/*
		 * Per RFC 6066 Section 3, the only permitted values in the
		 * TLS server_name (SNI) field are FQDNs (DNS hostnames).
		 * IPv4 and IPv6 addresses, port numbers are not allowed.
		 */
		if (!is_ip_address(servername)) {
			argv[5] = (char*)"-servername";
			argv[6] = (char*)servername;
		}

		BB_EXECVP(argv[0], argv);
		xmove_fd(3, 2);
# if ENABLE_FEATURE_WGET_HTTPS
		child_failed = 1;
		xfunc_die();
# else
		bb_perror_msg_and_die("can't execute '%s'", argv[0]);
# endif
		/* notreached */
	}

	/* Parent */
	free(servername);
	free(allocated);
	close(sp[1]);
# if ENABLE_FEATURE_WGET_HTTPS
	if (child_failed) {
		close(sp[0]);
		return -1;
	}
# endif
	return sp[0];
}