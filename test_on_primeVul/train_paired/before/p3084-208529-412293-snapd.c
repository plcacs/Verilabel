static void setup_private_mount(const char *snap_name)
{
	uid_t uid = getuid();
	gid_t gid = getgid();
	char tmpdir[MAX_BUF] = { 0 };

	// Create a 0700 base directory, this is the base dir that is
	// protected from other users.
	//
	// Under that basedir, we put a 1777 /tmp dir that is then bind
	// mounted for the applications to use
	sc_must_snprintf(tmpdir, sizeof(tmpdir), "/tmp/snap.%s_XXXXXX", snap_name);
	if (mkdtemp(tmpdir) == NULL) {
		die("cannot create temporary directory essential for private /tmp");
	}
	// now we create a 1777 /tmp inside our private dir
	mode_t old_mask = umask(0);
	char *d = sc_strdup(tmpdir);
	sc_must_snprintf(tmpdir, sizeof(tmpdir), "%s/tmp", d);
	free(d);

	if (mkdir(tmpdir, 01777) != 0) {
		die("cannot create temporary directory for private /tmp");
	}
	umask(old_mask);

	// chdir to '/' since the mount won't apply to the current directory
	char *pwd = get_current_dir_name();
	if (pwd == NULL)
		die("cannot get current working directory");
	if (chdir("/") != 0)
		die("cannot change directory to '/'");

	// MS_BIND is there from linux 2.4
	sc_do_mount(tmpdir, "/tmp", NULL, MS_BIND, NULL);
	// MS_PRIVATE needs linux > 2.6.11
	sc_do_mount("none", "/tmp", NULL, MS_PRIVATE, NULL);
	// do the chown after the bind mount to avoid potential shenanigans
	if (chown("/tmp/", uid, gid) < 0) {
		die("cannot change ownership of /tmp");
	}
	// chdir to original directory
	if (chdir(pwd) != 0)
		die("cannot change current working directory to the original directory");
	free(pwd);
}