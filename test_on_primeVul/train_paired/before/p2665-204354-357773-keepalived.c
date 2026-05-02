FILE *fopen_safe(const char *path, const char *mode)
{
	int fd;
	FILE *file;
	int flags = O_NOFOLLOW | O_CREAT;
	int sav_errno;

	if (mode[0] == 'r')
		return fopen(path, mode);

	if ((mode[0] != 'a' && mode[0] != 'w') ||
	    (mode[1] &&
	     (mode[1] != '+' || mode[2]))) {
		errno = EINVAL;
		return NULL;
	}

	if (mode[0] == 'w')
		flags |= O_TRUNC;
	else
		flags |= O_APPEND;

	if (mode[1])
		flags |= O_RDWR;
	else
		flags |= O_WRONLY;

	if (mode[0] == 'w') {
		/* Ensure that any existing file isn't currently opened for read by a non-privileged user.
		 * We do this by unlinking the file, so that the open() below will create a new file. */
		if (unlink(path) && errno != ENOENT) {
			log_message(LOG_INFO, "Failed to remove existing file '%s' prior to write", path);
			return NULL;
		}
	}
	else {
		/* Only allow append mode if debugging features requiring append are enabled. Since we
		 * can't unlink the file, there may be a non privileged user who already has the file open
		 * for read (e.g. tail -f). If these debug option aren't enabled, there is no potential
		 * security risk. */
#ifndef ENABLE_LOG_FILE_APPEND
		log_message(LOG_INFO, "BUG - shouldn't be opening file for append with current build options");
		errno = EINVAL;
		return NULL;
#endif
	}

	fd = open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (fd == -1)
		return NULL;

	/* Change file ownership to root */
	if (fchown(fd, 0, 0)) {
		sav_errno = errno;
		log_message(LOG_INFO, "Unable to change file ownership of %s- errno %d (%m)", path, errno);
		close(fd);
		errno = sav_errno;
		return NULL;
	}

	/* Set file mode to rw------- */
	if (fchmod(fd, S_IRUSR | S_IWUSR)) {
		sav_errno = errno;
		log_message(LOG_INFO, "Unable to change file permission of %s - errno %d (%m)", path, errno);
		close(fd);
		errno = sav_errno;
		return NULL;
	}

	file = fdopen (fd, "w");
	if (!file) {
		sav_errno = errno;
		log_message(LOG_INFO, "fdopen(\"%s\") failed - errno %d (%m)", path, errno);
		close(fd);
		errno = sav_errno;
		return NULL;
	}

	return file;
}