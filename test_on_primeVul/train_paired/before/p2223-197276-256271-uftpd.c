char *compose_path(ctrl_t *ctrl, char *path)
{
	struct stat st;
	static char rpath[PATH_MAX];
	char *name, *ptr;
	char dir[PATH_MAX] = { 0 };

	strlcpy(dir, ctrl->cwd, sizeof(dir));
	DBG("Compose path from cwd: %s, arg: %s", ctrl->cwd, path ?: "");
	if (!path || !strlen(path))
		goto check;

	if (path) {
		if (path[0] != '/') {
			if (dir[strlen(dir) - 1] != '/')
				strlcat(dir, "/", sizeof(dir));
		}
		strlcat(dir, path, sizeof(dir));
	}

check:
	while ((ptr = strstr(dir, "//")))
		memmove(ptr, &ptr[1], strlen(&ptr[1]) + 1);

	if (!chrooted) {
		size_t len = strlen(home);

		DBG("Server path from CWD: %s", dir);
		if (len > 0 && home[len - 1] == '/')
			len--;
		memmove(dir + len, dir, strlen(dir) + 1);
		memcpy(dir, home, len);
		DBG("Resulting non-chroot path: %s", dir);
	}

	/*
	 * Handle directories slightly differently, since dirname() on a
	 * directory returns the parent directory.  So, just squash ..
	 */
	if (!stat(dir, &st) && S_ISDIR(st.st_mode)) {
		if (!realpath(dir, rpath))
			return NULL;
	} else {
		/*
		 * Check realpath() of directory containing the file, a
		 * STOR may want to save a new file.  Then append the
		 * file and return it.
		 */
		name = basename(path);
		ptr = dirname(dir);

		memset(rpath, 0, sizeof(rpath));
		if (!realpath(ptr, rpath)) {
			INFO("Failed realpath(%s): %m", ptr);
			return NULL;
		}

		if (rpath[1] != 0)
			strlcat(rpath, "/", sizeof(rpath));
		strlcat(rpath, name, sizeof(rpath));
	}

	if (!chrooted && strncmp(dir, home, strlen(home))) {
		DBG("Failed non-chroot dir:%s vs home:%s", dir, home);
		return NULL;
	}

	return rpath;
}