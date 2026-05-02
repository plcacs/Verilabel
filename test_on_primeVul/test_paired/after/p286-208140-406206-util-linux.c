int main(int argc, char **argv)
{
	int c, rc = MOUNT_EX_SUCCESS, all = 0, show_labels = 0;
	struct libmnt_context *cxt;
	struct libmnt_table *fstab = NULL;
	char *srcbuf = NULL;
	char *types = NULL;
	unsigned long oper = 0;

	enum {
		MOUNT_OPT_SHARED = CHAR_MAX + 1,
		MOUNT_OPT_SLAVE,
		MOUNT_OPT_PRIVATE,
		MOUNT_OPT_UNBINDABLE,
		MOUNT_OPT_RSHARED,
		MOUNT_OPT_RSLAVE,
		MOUNT_OPT_RPRIVATE,
		MOUNT_OPT_RUNBINDABLE,
		MOUNT_OPT_TARGET,
		MOUNT_OPT_SOURCE
	};

	static const struct option longopts[] = {
		{ "all", 0, 0, 'a' },
		{ "fake", 0, 0, 'f' },
		{ "fstab", 1, 0, 'T' },
		{ "fork", 0, 0, 'F' },
		{ "help", 0, 0, 'h' },
		{ "no-mtab", 0, 0, 'n' },
		{ "read-only", 0, 0, 'r' },
		{ "ro", 0, 0, 'r' },
		{ "verbose", 0, 0, 'v' },
		{ "version", 0, 0, 'V' },
		{ "read-write", 0, 0, 'w' },
		{ "rw", 0, 0, 'w' },
		{ "options", 1, 0, 'o' },
		{ "test-opts", 1, 0, 'O' },
		{ "pass-fd", 1, 0, 'p' },
		{ "types", 1, 0, 't' },
		{ "uuid", 1, 0, 'U' },
		{ "label", 1, 0, 'L'},
		{ "bind", 0, 0, 'B' },
		{ "move", 0, 0, 'M' },
		{ "rbind", 0, 0, 'R' },
		{ "make-shared", 0, 0, MOUNT_OPT_SHARED },
		{ "make-slave", 0, 0, MOUNT_OPT_SLAVE },
		{ "make-private", 0, 0, MOUNT_OPT_PRIVATE },
		{ "make-unbindable", 0, 0, MOUNT_OPT_UNBINDABLE },
		{ "make-rshared", 0, 0, MOUNT_OPT_RSHARED },
		{ "make-rslave", 0, 0, MOUNT_OPT_RSLAVE },
		{ "make-rprivate", 0, 0, MOUNT_OPT_RPRIVATE },
		{ "make-runbindable", 0, 0, MOUNT_OPT_RUNBINDABLE },
		{ "no-canonicalize", 0, 0, 'c' },
		{ "internal-only", 0, 0, 'i' },
		{ "show-labels", 0, 0, 'l' },
		{ "target", 1, 0, MOUNT_OPT_TARGET },
		{ "source", 1, 0, MOUNT_OPT_SOURCE },
		{ NULL, 0, 0, 0 }
	};

	static const ul_excl_t excl[] = {       /* rows and cols in in ASCII order */
		{ 'B','M','R',			/* bind,move,rbind */
		   MOUNT_OPT_SHARED,   MOUNT_OPT_SLAVE,
		   MOUNT_OPT_PRIVATE,  MOUNT_OPT_UNBINDABLE,
		   MOUNT_OPT_RSHARED,  MOUNT_OPT_RSLAVE,
		   MOUNT_OPT_RPRIVATE, MOUNT_OPT_RUNBINDABLE },

		{ 'L','U', MOUNT_OPT_SOURCE },	/* label,uuid,source */
		{ 0 }
	};
	int excl_st[ARRAY_SIZE(excl)] = UL_EXCL_STATUS_INIT;

	sanitize_env();
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	mnt_init_debug(0);
	cxt = mnt_new_context();
	if (!cxt)
		err(MOUNT_EX_SYSERR, _("libmount context allocation failed"));

	mnt_context_set_tables_errcb(cxt, table_parser_errcb);

	while ((c = getopt_long(argc, argv, "aBcfFhilL:Mno:O:p:rRsU:vVwt:T:",
					longopts, NULL)) != -1) {

		/* only few options are allowed for non-root users */
		if (mnt_context_is_restricted(cxt) &&
		    !strchr("hlLUVvpris", c) &&
		    c != MOUNT_OPT_TARGET &&
		    c != MOUNT_OPT_SOURCE)
			exit_non_root(option_to_longopt(c, longopts));

		err_exclusive_options(c, longopts, excl, excl_st);

		switch(c) {
		case 'a':
			all = 1;
			break;
		case 'c':
			mnt_context_disable_canonicalize(cxt, TRUE);
			break;
		case 'f':
			mnt_context_enable_fake(cxt, TRUE);
			break;
		case 'F':
			mnt_context_enable_fork(cxt, TRUE);
			break;
		case 'h':
			usage(stdout);
			break;
		case 'i':
			mnt_context_disable_helpers(cxt, TRUE);
			break;
		case 'n':
			mnt_context_disable_mtab(cxt, TRUE);
			break;
		case 'r':
			if (mnt_context_append_options(cxt, "ro"))
				err(MOUNT_EX_SYSERR, _("failed to append options"));
			readwrite = 0;
			break;
		case 'v':
			mnt_context_enable_verbose(cxt, TRUE);
			break;
		case 'V':
			print_version();
			break;
		case 'w':
			if (mnt_context_append_options(cxt, "rw"))
				err(MOUNT_EX_SYSERR, _("failed to append options"));
			readwrite = 1;
			break;
		case 'o':
			if (mnt_context_append_options(cxt, optarg))
				err(MOUNT_EX_SYSERR, _("failed to append options"));
			break;
		case 'O':
			if (mnt_context_set_options_pattern(cxt, optarg))
				err(MOUNT_EX_SYSERR, _("failed to set options pattern"));
			break;
		case 'p':
                        warnx(_("--pass-fd is no longer supported"));
			break;
		case 'L':
			xasprintf(&srcbuf, "LABEL=\"%s\"", optarg);
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_source(cxt, srcbuf);
			free(srcbuf);
			break;
		case 'U':
			xasprintf(&srcbuf, "UUID=\"%s\"", optarg);
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_source(cxt, srcbuf);
			free(srcbuf);
			break;
		case 'l':
			show_labels = 1;
			break;
		case 't':
			types = optarg;
			break;
		case 'T':
			fstab = append_fstab(cxt, fstab, optarg);
			break;
		case 's':
			mnt_context_enable_sloppy(cxt, TRUE);
			break;
		case 'B':
			oper |= MS_BIND;
			break;
		case 'M':
			oper |= MS_MOVE;
			break;
		case 'R':
			oper |= (MS_BIND | MS_REC);
			break;
		case MOUNT_OPT_SHARED:
			oper |= MS_SHARED;
			break;
		case MOUNT_OPT_SLAVE:
			oper |= MS_SLAVE;
			break;
		case MOUNT_OPT_PRIVATE:
			oper |= MS_PRIVATE;
			break;
		case MOUNT_OPT_UNBINDABLE:
			oper |= MS_UNBINDABLE;
			break;
		case MOUNT_OPT_RSHARED:
			oper |= (MS_SHARED | MS_REC);
			break;
		case MOUNT_OPT_RSLAVE:
			oper |= (MS_SLAVE | MS_REC);
			break;
		case MOUNT_OPT_RPRIVATE:
			oper |= (MS_PRIVATE | MS_REC);
			break;
		case MOUNT_OPT_RUNBINDABLE:
			oper |= (MS_UNBINDABLE | MS_REC);
			break;
		case MOUNT_OPT_TARGET:
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_target(cxt, optarg);
			break;
		case MOUNT_OPT_SOURCE:
			mnt_context_disable_swapmatch(cxt, 1);
			mnt_context_set_source(cxt, optarg);
			break;
		default:
			usage(stderr);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (fstab && !mnt_context_is_nocanonicalize(cxt)) {
		/*
		 * We have external (context independent) fstab instance, let's
		 * make a connection between the fstab and the canonicalization
		 * cache.
		 */
		struct libmnt_cache *cache = mnt_context_get_cache(cxt);
		mnt_table_set_cache(fstab, cache);
	}

	if (!mnt_context_get_source(cxt) &&
	    !mnt_context_get_target(cxt) &&
	    !argc &&
	    !all) {
		if (oper)
			usage(stderr);
		print_all(cxt, types, show_labels);
		goto done;
	}

	if (oper && (types || all || mnt_context_get_source(cxt)))
		usage(stderr);

	if (types && (all || strchr(types, ',') ||
			     strncmp(types, "no", 2) == 0))
		mnt_context_set_fstype_pattern(cxt, types);
	else if (types)
		mnt_context_set_fstype(cxt, types);

	if (all) {
		/*
		 * A) Mount all
		 */
		rc = mount_all(cxt);
		goto done;

	} else if (argc == 0 && (mnt_context_get_source(cxt) ||
				 mnt_context_get_target(cxt))) {
		/*
		 * B) mount -L|-U|--source|--target
		 */
		if (mnt_context_is_restricted(cxt) &&
		    mnt_context_get_source(cxt) &&
		    mnt_context_get_target(cxt))
			exit_non_root(NULL);

	} else if (argc == 1) {
		/*
		 * C) mount [-L|-U|--source] <target>
		 *    mount <source|target>
		 *
		 * non-root may specify source *or* target, but not both
		 */
		if (mnt_context_is_restricted(cxt) &&
		    mnt_context_get_source(cxt))
			exit_non_root(NULL);

		mnt_context_set_target(cxt, argv[0]);

	} else if (argc == 2 && !mnt_context_get_source(cxt)
			     && !mnt_context_get_target(cxt)) {
		/*
		 * D) mount <source> <target>
		 */
		if (mnt_context_is_restricted(cxt))
			exit_non_root(NULL);
		mnt_context_set_source(cxt, argv[0]);
		mnt_context_set_target(cxt, argv[1]);

	} else
		usage(stderr);

	if (mnt_context_is_restricted(cxt))
		sanitize_paths(cxt);

	if (oper) {
		/* MS_PROPAGATION operations, let's set the mount flags */
		mnt_context_set_mflags(cxt, oper);

		/* For -make* or --bind is fstab unnecessary */
		mnt_context_set_optsmode(cxt, MNT_OMODE_NOTAB);
	}

	rc = mnt_context_mount(cxt);
	rc = mk_exit_code(cxt, rc);

	if (rc == MOUNT_EX_SUCCESS && mnt_context_is_verbose(cxt))
		success_message(cxt);
done:
	mnt_free_context(cxt);
	mnt_free_table(fstab);
	return rc;
}