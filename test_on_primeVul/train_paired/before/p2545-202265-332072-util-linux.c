umount_one (const char *spec, const char *node, const char *type,
	    const char *opts, struct mntentchn *mc) {
	int umnt_err = 0;
	int isroot;
	int res = 0;
	int status;
	int extra_flags = 0;
	const char *loopdev, *target = node;
	char *targetbuf = NULL;
	int myloop = 0;

	/* Special case for root.  As of 0.99pl10 we can (almost) unmount root;
	   the kernel will remount it readonly so that we can carry on running
	   afterwards.  The readonly remount is illegal if any files are opened
	   for writing at the time, so we can't update mtab for an unmount of
	   root.  As it is only really a remount, this doesn't matter too
	   much.  [sct May 29, 1993] */
	isroot = (streq (node, "/") || streq (node, "root")
		  || streq (node, "rootfs"));
	if (isroot)
		nomtab++;

	/*
	 * Call umount.TYPE for types that require a separate umount program.
	 * All such special things must occur isolated in the types string.
	 */
	if (check_special_umountprog(spec, node, type, &status))
		return status;

	/* Skip the actual umounting for --fake */
	if (fake)
		goto writemtab;
	/*
	 * Ignore the option "-d" for non-loop devices and loop devices with
	 * LO_FLAGS_AUTOCLEAR flag.
	 */
	if (delloop && is_loop_device(spec))
		myloop = 1;

	if (restricted) {
		if (umount_nofollow_support())
			extra_flags |= UMOUNT_NOFOLLOW;

		/* call umount(2) with relative path to avoid races */
		target = chdir_to_parent(node, &targetbuf);
	}

	if (lazy) {
		res = umount2 (target, MNT_DETACH | extra_flags);
		if (res < 0)
			umnt_err = errno;
		goto writemtab;
	}

	if (force) {		/* only supported for NFS */
		res = umount2 (target, MNT_FORCE | extra_flags);
		if (res == -1) {
			int errsv = errno;
			perror("umount2");
			errno = errsv;
			if (errno == ENOSYS) {
				if (verbose)
					printf(_("no umount2, trying umount...\n"));
				res = umount (target);
			}
		}
	} else if (extra_flags)
		res = umount2 (target, extra_flags);
	else
		res = umount (target);

	free(targetbuf);

	if (res < 0)
		umnt_err = errno;

	if (res < 0 && remount && umnt_err == EBUSY) {
		/* Umount failed - let us try a remount */
		res = mount(spec, node, NULL,
			    MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, NULL);
		if (res == 0) {
			fprintf(stderr,
				_("umount: %s busy - remounted read-only\n"),
				spec);
			if (mc && !nomtab) {
				/* update mtab if the entry is there */
				struct my_mntent remnt;
				remnt.mnt_fsname = mc->m.mnt_fsname;
				remnt.mnt_dir = mc->m.mnt_dir;
				remnt.mnt_type = mc->m.mnt_type;
				remnt.mnt_opts = "ro";
				remnt.mnt_freq = 0;
				remnt.mnt_passno = 0;
				update_mtab(node, &remnt);
			}
			return 0;
		} else if (errno != EBUSY) { 	/* hmm ... */
			perror("remount");
			fprintf(stderr,
				_("umount: could not remount %s read-only\n"),
				spec);
		}
	}

	loopdev = 0;
	if (res >= 0) {
		/* Umount succeeded */
		if (verbose)
			printf (_("%s has been unmounted\n"), spec);

		/* Free any loop devices that we allocated ourselves */
		if (mc) {
			char *optl;

			/* old style mtab line? */
			if (streq(mc->m.mnt_type, "loop")) {
				loopdev = spec;
				goto gotloop;
			}

			/* new style mtab line? */
			optl = mc->m.mnt_opts ? xstrdup(mc->m.mnt_opts) : "";
			for (optl = strtok (optl, ","); optl;
			     optl = strtok (NULL, ",")) {
				if (!strncmp(optl, "loop=", 5)) {
					loopdev = optl+5;
					goto gotloop;
				}
			}
		} else {
			/*
			 * If option "-o loop=spec" occurs in mtab,
			 * note the mount point, and delete mtab line.
			 */
			if ((mc = getmntoptfile (spec)) != NULL)
				node = mc->m.mnt_dir;
		}

		/* Also free loop devices when -d flag is given */
		if (myloop)
			loopdev = spec;
	}
 gotloop:
	if (loopdev && !is_loop_autoclear(loopdev))
		del_loop(loopdev);

 writemtab:
	if (!nomtab &&
	    (umnt_err == 0 || umnt_err == EINVAL || umnt_err == ENOENT)) {
#ifdef HAVE_LIBMOUNT_MOUNT
		struct libmnt_update *upd = mnt_new_update();

		if (upd && !mnt_update_set_fs(upd, 0, node, NULL)) {
			struct libmnt_lock *lc = init_libmount_lock(
						mnt_update_get_filename(upd));
			mnt_update_table(upd, lc);
			init_libmount_lock(NULL);
		}
		mnt_free_update(upd);
#else
		update_mtab (node, NULL);
#endif
	}

	if (res >= 0)
		return 0;
	if (umnt_err)
		complain(umnt_err, node);
	return 1;
}