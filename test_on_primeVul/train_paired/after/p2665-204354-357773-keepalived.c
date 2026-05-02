FILE *fopen_safe(const char *path, const char *mode)
{
	int fd;
	FILE *file;
	int flags = O_NOFOLLOW | O_CREAT | O_CLOEXEC;
	int sav_errno;
	char file_tmp_name[] = "/tmp/keepalivedXXXXXX";

	if (mode[0] == 'r')
		return fopen(path, mode);

	if ((mode[0] != 'a' && mode[0] != 'w') ||
	    (mode[1] &&
	     (mode[1] != '+' || mode[2]))) {
		errno = EINVAL;
		return NULL;
	}

	if (mode[0] == 'w') {
		/* If we truncate an existing file, any non-privileged user who already has the file
		 * open would be able to read what we write, even though the file access mode is changed.
		 *
		 * If we unlink an existing file and the desired file is subsequently created via open,
		 * it leaves a window for someone else to create the same file between the unlink and the open.
		 *
		 * The solution is to create a temporary file that we will rename to the desired file name.
		 * Since the temporary file is created owned by root with the only file access persmissions being
		 * owner read and write, no non root user will have access to the file. Further, the rename to
		 * the requested filename is atomic, and so there is no window when someone else could create
		 * another file of the same name.
		 */
		fd = mkostemp(file_tmp_name, O_CLOEXEC);
	} else {
		/* Only allow append mode if debugging features requiring append are enabled. Since we
		 * can't unlink the file, there may be a non privileged user who already has the file open
		 * for read (e.g. tail -f). If these debug option aren't enabled, there is no potential
		 * security risk in that respect. */
#ifndef ENABLE_LOG_FILE_APPEND
		log_message(LOG_INFO, "BUG - shouldn't be opening file for append with current build options");
		errno = EINVAL;
		return NULL;
#else
		flags = O_NOFOLLOW | O_CREAT | O_CLOEXEC | O_APPEND;

		if (mode[1])
			flags |= O_RDWR;
		else
			flags |= O_WRONLY;

		fd = open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
#endif
	}

	if (fd == -1) {
		sav_errno = errno;
		log_message(LOG_INFO, "Unable to open '%s' - errno %d (%m)", path, errno);
		errno = sav_errno;
		return NULL;
	}

#ifndef ENABLE_LOG_FILE_APPEND
	/* Change file ownership to root */
	if (mode[0] == 'a' && fchown(fd, 0, 0)) {
		sav_errno = errno;
		log_message(LOG_INFO, "Unable to change file ownership of %s- errno %d (%m)", path, errno);
		close(fd);
		errno = sav_errno;
		return NULL;
	}
#endif

	/* Set file mode to rw------- */
	if (fchmod(fd, S_IRUSR | S_IWUSR)) {
		sav_errno = errno;
		log_message(LOG_INFO, "Unable to change file permission of %s - errno %d (%m)", path, errno);
		close(fd);
		errno = sav_errno;
		return NULL;
	}

	if (mode[0] == 'w') {
		/* Rename the temporary file to the one we want */
		if (rename(file_tmp_name, path)) {
			sav_errno = errno;
			log_message(LOG_INFO, "Failed to rename %s to %s - errno %d (%m)", file_tmp_name, path, errno);
			close(fd);
			errno = sav_errno;
			return NULL;
		}
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