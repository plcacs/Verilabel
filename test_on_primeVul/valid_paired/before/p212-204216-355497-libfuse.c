static int do_mount(const char *mnt, char **typep, mode_t rootmode,
		    int fd, const char *opts, const char *dev, char **sourcep,
		    char **mnt_optsp)
{
	int res;
	int flags = MS_NOSUID | MS_NODEV;
	char *optbuf;
	char *mnt_opts = NULL;
	const char *s;
	char *d;
	char *fsname = NULL;
	char *subtype = NULL;
	char *source = NULL;
	char *type = NULL;
	int blkdev = 0;

	optbuf = (char *) malloc(strlen(opts) + 128);
	if (!optbuf) {
		fprintf(stderr, "%s: failed to allocate memory\n", progname);
		return -1;
	}

	for (s = opts, d = optbuf; *s;) {
		unsigned len;
		const char *fsname_str = "fsname=";
		const char *subtype_str = "subtype=";
		bool escape_ok = begins_with(s, fsname_str) ||
				 begins_with(s, subtype_str);
		for (len = 0; s[len]; len++) {
			if (escape_ok && s[len] == '\\' && s[len + 1])
				len++;
			else if (s[len] == ',')
				break;
		}
		if (begins_with(s, fsname_str)) {
			if (!get_string_opt(s, len, fsname_str, &fsname))
				goto err;
		} else if (begins_with(s, subtype_str)) {
			if (!get_string_opt(s, len, subtype_str, &subtype))
				goto err;
		} else if (opt_eq(s, len, "blkdev")) {
			if (getuid() != 0) {
				fprintf(stderr,
					"%s: option blkdev is privileged\n",
					progname);
				goto err;
			}
			blkdev = 1;
		} else if (opt_eq(s, len, "auto_unmount")) {
			auto_unmount = 1;
		} else if (!begins_with(s, "fd=") &&
			   !begins_with(s, "rootmode=") &&
			   !begins_with(s, "user_id=") &&
			   !begins_with(s, "group_id=")) {
			int on;
			int flag;
			int skip_option = 0;
			if (opt_eq(s, len, "large_read")) {
				struct utsname utsname;
				unsigned kmaj, kmin;
				res = uname(&utsname);
				if (res == 0 &&
				    sscanf(utsname.release, "%u.%u",
					   &kmaj, &kmin) == 2 &&
				    (kmaj > 2 || (kmaj == 2 && kmin > 4))) {
					fprintf(stderr, "%s: note: 'large_read' mount option is deprecated for %i.%i kernels\n", progname, kmaj, kmin);
					skip_option = 1;
				}
			}
			if (getuid() != 0 && !user_allow_other &&
			    (opt_eq(s, len, "allow_other") ||
			     opt_eq(s, len, "allow_root"))) {
				fprintf(stderr, "%s: option %.*s only allowed if 'user_allow_other' is set in %s\n", progname, len, s, FUSE_CONF);
				goto err;
			}
			if (!skip_option) {
				if (find_mount_flag(s, len, &on, &flag)) {
					if (on)
						flags |= flag;
					else
						flags  &= ~flag;
				} else {
					memcpy(d, s, len);
					d += len;
					*d++ = ',';
				}
			}
		}
		s += len;
		if (*s)
			s++;
	}
	*d = '\0';
	res = get_mnt_opts(flags, optbuf, &mnt_opts);
	if (res == -1)
		goto err;

	sprintf(d, "fd=%i,rootmode=%o,user_id=%u,group_id=%u",
		fd, rootmode, getuid(), getgid());

	source = malloc((fsname ? strlen(fsname) : 0) +
			(subtype ? strlen(subtype) : 0) + strlen(dev) + 32);

	type = malloc((subtype ? strlen(subtype) : 0) + 32);
	if (!type || !source) {
		fprintf(stderr, "%s: failed to allocate memory\n", progname);
		goto err;
	}

	if (subtype)
		sprintf(type, "%s.%s", blkdev ? "fuseblk" : "fuse", subtype);
	else
		strcpy(type, blkdev ? "fuseblk" : "fuse");

	if (fsname)
		strcpy(source, fsname);
	else
		strcpy(source, subtype ? subtype : dev);

	res = mount_notrunc(source, mnt, type, flags, optbuf);
	if (res == -1 && errno == ENODEV && subtype) {
		/* Probably missing subtype support */
		strcpy(type, blkdev ? "fuseblk" : "fuse");
		if (fsname) {
			if (!blkdev)
				sprintf(source, "%s#%s", subtype, fsname);
		} else {
			strcpy(source, type);
		}

		res = mount_notrunc(source, mnt, type, flags, optbuf);
	}
	if (res == -1 && errno == EINVAL) {
		/* It could be an old version not supporting group_id */
		sprintf(d, "fd=%i,rootmode=%o,user_id=%u",
			fd, rootmode, getuid());
		res = mount_notrunc(source, mnt, type, flags, optbuf);
	}
	if (res == -1) {
		int errno_save = errno;
		if (blkdev && errno == ENODEV && !fuse_mnt_check_fuseblk())
			fprintf(stderr, "%s: 'fuseblk' support missing\n",
				progname);
		else
			fprintf(stderr, "%s: mount failed: %s\n", progname,
				strerror(errno_save));
		goto err;
	}
	*sourcep = source;
	*typep = type;
	*mnt_optsp = mnt_opts;
	free(fsname);
	free(optbuf);

	return 0;

err:
	free(fsname);
	free(subtype);
	free(source);
	free(type);
	free(mnt_opts);
	free(optbuf);
	return -1;
}