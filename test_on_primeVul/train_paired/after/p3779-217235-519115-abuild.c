int main(int argc, const char *argv[])
{
	struct group *grent;
	const char *cmd;
	const char *path;
	int i;
	struct passwd *pw;

	grent = getgrnam(ABUILD_GROUP);
	if (grent == NULL)
		errx(1, "%s: Group not found", ABUILD_GROUP);

	char *name = NULL;
	pw = getpwuid(getuid());
	if (pw)
		name = pw->pw_name;

	if (!is_in_group(grent->gr_gid)) {
		errx(1, "User %s is not a member of group %s\n",
			name ? name : "(unknown)", ABUILD_GROUP);
	}

	if (name == NULL)
		warnx("Could not find username for uid %d\n", getuid());
	setenv("USER", name ?: "", 1);

	cmd = strrchr(argv[0], '/');
	if (cmd)
		cmd++;
	else
		cmd = argv[0];
	cmd = strchr(cmd, '-');
	if (cmd == NULL)
		errx(1, "Calling command has no '-'");
	cmd++;

	path = get_command_path(cmd);
	if (path == NULL)
		errx(1, "%s: Not a valid subcommand", cmd);

	for (i = 1; i < argc; i++)
		check_option(argv[i]);

	argv[0] = path;
	/* set our uid to root so bbsuid --install works */
	setuid(0);
	/* set our gid to root so apk commit hooks run with the same gid as for "sudo apk add ..." */
	setgid(0);
	execv(path, (char * const*)argv);
	perror(path);
	return 1;
}