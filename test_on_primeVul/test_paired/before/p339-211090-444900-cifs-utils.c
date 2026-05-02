add_mtab(char *devname, char *mountpoint, unsigned long flags, const char *fstype)
{
	int rc = 0;
	uid_t uid;
	char *mount_user = NULL;
	struct mntent mountent;
	FILE *pmntfile;
	sigset_t mask, oldmask;

	uid = getuid();
	if (uid != 0)
		mount_user = getusername(uid);

	/*
	 * Set the real uid to the effective uid. This prevents unprivileged
	 * users from sending signals to this process, though ^c on controlling
	 * terminal should still work.
	 */
	rc = setreuid(geteuid(), -1);
	if (rc != 0) {
		fprintf(stderr, "Unable to set real uid to effective uid: %s\n",
				strerror(errno));
		return EX_FILEIO;
	}

	rc = sigfillset(&mask);
	if (rc) {
		fprintf(stderr, "Unable to set filled signal mask\n");
		return EX_FILEIO;
	}

	rc = sigprocmask(SIG_SETMASK, &mask, &oldmask);
	if (rc) {
		fprintf(stderr, "Unable to make process ignore signals\n");
		return EX_FILEIO;
	}

	rc = toggle_dac_capability(1, 1);
	if (rc)
		return EX_FILEIO;

	atexit(unlock_mtab);
	rc = lock_mtab();
	if (rc) {
		fprintf(stderr, "cannot lock mtab");
		rc = EX_FILEIO;
		goto add_mtab_exit;
	}

	pmntfile = setmntent(MOUNTED, "a+");
	if (!pmntfile) {
		fprintf(stderr, "could not update mount table\n");
		unlock_mtab();
		rc = EX_FILEIO;
		goto add_mtab_exit;
	}

	mountent.mnt_fsname = devname;
	mountent.mnt_dir = mountpoint;
	mountent.mnt_type = (char *)(void *)fstype;
	mountent.mnt_opts = (char *)calloc(MTAB_OPTIONS_LEN, 1);
	if (mountent.mnt_opts) {
		if (flags & MS_RDONLY)
			strlcat(mountent.mnt_opts, "ro", MTAB_OPTIONS_LEN);
		else
			strlcat(mountent.mnt_opts, "rw", MTAB_OPTIONS_LEN);

		if (flags & MS_MANDLOCK)
			strlcat(mountent.mnt_opts, ",mand", MTAB_OPTIONS_LEN);
		if (flags & MS_NOEXEC)
			strlcat(mountent.mnt_opts, ",noexec", MTAB_OPTIONS_LEN);
		if (flags & MS_NOSUID)
			strlcat(mountent.mnt_opts, ",nosuid", MTAB_OPTIONS_LEN);
		if (flags & MS_NODEV)
			strlcat(mountent.mnt_opts, ",nodev", MTAB_OPTIONS_LEN);
		if (flags & MS_SYNCHRONOUS)
			strlcat(mountent.mnt_opts, ",sync", MTAB_OPTIONS_LEN);
		if (mount_user) {
			strlcat(mountent.mnt_opts, ",user=", MTAB_OPTIONS_LEN);
			strlcat(mountent.mnt_opts, mount_user,
				MTAB_OPTIONS_LEN);
		}
	}
	mountent.mnt_freq = 0;
	mountent.mnt_passno = 0;
	rc = addmntent(pmntfile, &mountent);
	if (rc) {
		fprintf(stderr, "unable to add mount entry to mtab\n");
		rc = EX_FILEIO;
	}
	endmntent(pmntfile);
	unlock_mtab();
	SAFE_FREE(mountent.mnt_opts);
add_mtab_exit:
	toggle_dac_capability(1, 0);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);

	return rc;
}