int main(int argc, char **argv, char **envp)
{
	int opt;

	while ((opt = getopt(argc, argv, "b:h:k:p:q:w:z:xv")) != -1) {
		switch (opt) {
		case 'b':
			tmate_settings->bind_addr = xstrdup(optarg);
			break;
		case 'h':
			tmate_settings->tmate_host = xstrdup(optarg);
			break;
		case 'k':
			tmate_settings->keys_dir = xstrdup(optarg);
			break;
		case 'p':
			tmate_settings->ssh_port = atoi(optarg);
			break;
		case 'q':
			tmate_settings->ssh_port_advertized = atoi(optarg);
			break;
		case 'w':
			tmate_settings->websocket_hostname = xstrdup(optarg);
			break;
		case 'z':
			tmate_settings->websocket_port = atoi(optarg);
			break;
		case 'x':
			tmate_settings->use_proxy_protocol = true;
			break;
		case 'v':
			tmate_settings->log_level++;
			break;
		default:
			usage();
			return 1;
		}
	}

	init_logging(tmate_settings->log_level);

	setup_locale();

	if (!tmate_settings->tmate_host)
		tmate_settings->tmate_host = get_full_hostname();

	cmdline = *argv;
	cmdline_end = *envp;

	tmate_preload_trace_lib();
	tmate_catch_sigsegv();
	tmate_init_rand();

	if ((mkdir(TMATE_WORKDIR, 0700)             < 0 && errno != EEXIST) ||
	    (mkdir(TMATE_WORKDIR "/sessions", 0700) < 0 && errno != EEXIST) ||
	    (mkdir(TMATE_WORKDIR "/jail", 0700)     < 0 && errno != EEXIST))
		tmate_fatal("Cannot prepare session in " TMATE_WORKDIR);

	if ((chmod(TMATE_WORKDIR, 0700)             < 0) ||
	    (chmod(TMATE_WORKDIR "/sessions", 0700) < 0) ||
	    (chmod(TMATE_WORKDIR "/jail", 0700)     < 0))
		tmate_fatal("Cannot prepare session in " TMATE_WORKDIR);

	if (check_owned_directory_mode(TMATE_WORKDIR, 0700) ||
	    check_owned_directory_mode(TMATE_WORKDIR "/sessions", 0700) ||
	    check_owned_directory_mode(TMATE_WORKDIR "/jail", 0700))
		tmate_fatal(TMATE_WORKDIR " and subdirectories has incorrect ownership/mode. "
			    "Try deleting " TMATE_WORKDIR " and try again");

	tmate_ssh_server_main(tmate_session,
			      tmate_settings->keys_dir, tmate_settings->bind_addr, tmate_settings->ssh_port);
	return 0;
}