int main(int argc, char ** argv)
{
	int c;
	unsigned long flags = MS_MANDLOCK;
	char * orgoptions = NULL;
	char * share_name = NULL;
	const char * ipaddr = NULL;
	char * uuid = NULL;
	char * mountpoint = NULL;
	char * options = NULL;
	char * optionstail;
	char * resolved_path = NULL;
	char * temp;
	char * dev_name;
	int rc = 0;
	int rsize = 0;
	int wsize = 0;
	int nomtab = 0;
	int uid = 0;
	int gid = 0;
	int optlen = 0;
	int orgoptlen = 0;
	size_t options_size = 0;
	size_t current_len;
	int retry = 0; /* set when we have to retry mount with uppercase */
	struct addrinfo *addrhead = NULL, *addr;
	struct utsname sysinfo;
	struct mntent mountent;
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;
	FILE * pmntfile;

	/* setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE); */

	if(argc && argv)
		thisprogram = argv[0];
	else
		mount_cifs_usage(stderr);

	if(thisprogram == NULL)
		thisprogram = "mount.cifs";

	uname(&sysinfo);
	/* BB add workstation name and domain and pass down */

/* #ifdef _GNU_SOURCE
	fprintf(stderr, " node: %s machine: %s sysname %s domain %s\n", sysinfo.nodename,sysinfo.machine,sysinfo.sysname,sysinfo.domainname);
#endif */
	if(argc > 2) {
		dev_name = argv[1];
		share_name = strndup(argv[1], MAX_UNC_LEN);
		if (share_name == NULL) {
			fprintf(stderr, "%s: %s", argv[0], strerror(ENOMEM));
			exit(EX_SYSERR);
		}
		mountpoint = argv[2];
	} else if (argc == 2) {
		if ((strcmp(argv[1], "-V") == 0) ||
		    (strcmp(argv[1], "--version") == 0))
		{
			print_cifs_mount_version();
			exit(0);
		}

		if ((strcmp(argv[1], "-h") == 0) ||
		    (strcmp(argv[1], "-?") == 0) ||
		    (strcmp(argv[1], "--help") == 0))
			mount_cifs_usage(stdout);

		mount_cifs_usage(stderr);
	} else {
		mount_cifs_usage(stderr);
	}


	/* add sharename in opts string as unc= parm */
	while ((c = getopt_long (argc, argv, "afFhilL:no:O:rsSU:vVwt:",
			 longopts, NULL)) != -1) {
		switch (c) {
/* No code to do the following  options yet */
/*	case 'l':
		list_with_volumelabel = 1;
		break;
	case 'L':
		volumelabel = optarg;
		break; */
/*	case 'a':	       
		++mount_all;
		break; */

		case '?':
		case 'h':	 /* help */
			mount_cifs_usage(stdout);
		case 'n':
			++nomtab;
			break;
		case 'b':
#ifdef MS_BIND
			flags |= MS_BIND;
#else
			fprintf(stderr,
				"option 'b' (MS_BIND) not supported\n");
#endif
			break;
		case 'm':
#ifdef MS_MOVE		      
			flags |= MS_MOVE;
#else
			fprintf(stderr,
				"option 'm' (MS_MOVE) not supported\n");
#endif
			break;
		case 'o':
			orgoptions = strdup(optarg);
		    break;
		case 'r':  /* mount readonly */
			flags |= MS_RDONLY;
			break;
		case 'U':
			uuid = optarg;
			break;
		case 'v':
			++verboseflag;
			break;
		case 'V':
			print_cifs_mount_version();
			exit (0);
		case 'w':
			flags &= ~MS_RDONLY;
			break;
		case 'R':
			rsize = atoi(optarg) ;
			break;
		case 'W':
			wsize = atoi(optarg);
			break;
		case '1':
			if (isdigit(*optarg)) {
				char *ep;

				uid = strtoul(optarg, &ep, 10);
				if (*ep) {
					fprintf(stderr, "bad uid value \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
			} else {
				struct passwd *pw;

				if (!(pw = getpwnam(optarg))) {
					fprintf(stderr, "bad user name \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
				uid = pw->pw_uid;
				endpwent();
			}
			break;
		case '2':
			if (isdigit(*optarg)) {
				char *ep;

				gid = strtoul(optarg, &ep, 10);
				if (*ep) {
					fprintf(stderr, "bad gid value \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
			} else {
				struct group *gr;

				if (!(gr = getgrnam(optarg))) {
					fprintf(stderr, "bad user name \"%s\"\n", optarg);
					exit(EX_USAGE);
				}
				gid = gr->gr_gid;
				endpwent();
			}
			break;
		case 'u':
			got_user = 1;
			user_name = optarg;
			break;
		case 'd':
			domain_name = optarg; /* BB fix this - currently ignored */
			got_domain = 1;
			break;
		case 'p':
			if(mountpassword == NULL)
				mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
			if(mountpassword) {
				got_password = 1;
				strlcpy(mountpassword,optarg,MOUNT_PASSWD_SIZE+1);
			}
			break;
		case 'S':
			get_password_from_file(0 /* stdin */,NULL);
			break;
		case 't':
			break;
		case 'f':
			++fakemnt;
			break;
		default:
			fprintf(stderr, "unknown mount option %c\n",c);
			mount_cifs_usage(stderr);
		}
	}

	if((argc < 3) || (dev_name == NULL) || (mountpoint == NULL)) {
		mount_cifs_usage(stderr);
	}

	/* make sure mountpoint is legit */
	rc = chdir(mountpoint);
	if (rc) {
		fprintf(stderr, "Couldn't chdir to %s: %s\n", mountpoint,
				strerror(errno));
		rc = EX_USAGE;
		goto mount_exit;
	}

	rc = check_mountpoint(thisprogram, mountpoint);
	if (rc)
		goto mount_exit;

	/* sanity check for unprivileged mounts */
	if (getuid()) {
		rc = check_fstab(thisprogram, mountpoint, dev_name,
				 &orgoptions);
		if (rc)
			goto mount_exit;

		/* enable any default user mount flags */
		flags |= CIFS_SETUID_FLAGS;
	}

	if (getenv("PASSWD")) {
		if(mountpassword == NULL)
			mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
		if(mountpassword) {
			strlcpy(mountpassword,getenv("PASSWD"),MOUNT_PASSWD_SIZE+1);
			got_password = 1;
		}
	} else if (getenv("PASSWD_FD")) {
		get_password_from_file(atoi(getenv("PASSWD_FD")),NULL);
	} else if (getenv("PASSWD_FILE")) {
		get_password_from_file(0, getenv("PASSWD_FILE"));
	}

        if (orgoptions && parse_options(&orgoptions, &flags)) {
                rc = EX_USAGE;
		goto mount_exit;
	}

	if (getuid()) {
#if !CIFS_LEGACY_SETUID_CHECK
		if (!(flags & (MS_USERS|MS_USER))) {
			fprintf(stderr, "%s: permission denied\n", thisprogram);
			rc = EX_USAGE;
			goto mount_exit;
		}
#endif /* !CIFS_LEGACY_SETUID_CHECK */
		
		if (geteuid()) {
			fprintf(stderr, "%s: not installed setuid - \"user\" "
					"CIFS mounts not supported.",
					thisprogram);
			rc = EX_FAIL;
			goto mount_exit;
		}
	}

	flags &= ~(MS_USERS|MS_USER);

	addrhead = addr = parse_server(&share_name);
	if((addrhead == NULL) && (got_ip == 0)) {
		fprintf(stderr, "No ip address specified and hostname not found\n");
		rc = EX_USAGE;
		goto mount_exit;
	}
	
	/* BB save off path and pop after mount returns? */
	resolved_path = (char *)malloc(PATH_MAX+1);
	if (!resolved_path) {
		fprintf(stderr, "Unable to allocate memory.\n");
		rc = EX_SYSERR;
		goto mount_exit;
	}

	/* Note that if we can not canonicalize the name, we get
	   another chance to see if it is valid when we chdir to it */
	if(!realpath(".", resolved_path)) {
		fprintf(stderr, "Unable to resolve %s to canonical path: %s\n",
				mountpoint, strerror(errno));
		rc = EX_SYSERR;
		goto mount_exit;
	}

	mountpoint = resolved_path; 

	if(got_user == 0) {
		/* Note that the password will not be retrieved from the
		   USER env variable (ie user%password form) as there is
		   already a PASSWD environment varaible */
		if (getenv("USER"))
			user_name = strdup(getenv("USER"));
		if (user_name == NULL)
			user_name = getusername();
		got_user = 1;
	}
       
	if(got_password == 0) {
		char *tmp_pass = getpass("Password: "); /* BB obsolete sys call but
							   no good replacement yet. */
		mountpassword = (char *)calloc(MOUNT_PASSWD_SIZE+1,1);
		if (!tmp_pass || !mountpassword) {
			fprintf(stderr, "Password not entered, exiting\n");
			exit(EX_USAGE);
		}
		strlcpy(mountpassword, tmp_pass, MOUNT_PASSWD_SIZE+1);
		got_password = 1;
	}
	/* FIXME launch daemon (handles dfs name resolution and credential change) 
	   remember to clear parms and overwrite password field before launching */
	if(orgoptions) {
		optlen = strlen(orgoptions);
		orgoptlen = optlen;
	} else
		optlen = 0;
	if(share_name)
		optlen += strlen(share_name) + 4;
	else {
		fprintf(stderr, "No server share name specified\n");
		fprintf(stderr, "\nMounting the DFS root for server not implemented yet\n");
                exit(EX_USAGE);
	}
	if(user_name)
		optlen += strlen(user_name) + 6;
	optlen += MAX_ADDRESS_LEN + 4;
	if(mountpassword)
		optlen += strlen(mountpassword) + 6;
mount_retry:
	SAFE_FREE(options);
	options_size = optlen + 10 + DOMAIN_SIZE;
	options = (char *)malloc(options_size /* space for commas in password */ + 8 /* space for domain=  , domain name itself was counted as part of the length username string above */);

	if(options == NULL) {
		fprintf(stderr, "Could not allocate memory for mount options\n");
		exit(EX_SYSERR);
	}

	strlcpy(options, "unc=", options_size);
	strlcat(options,share_name,options_size);
	/* scan backwards and reverse direction of slash */
	temp = strrchr(options, '/');
	if(temp > options + 6)
		*temp = '\\';
	if(user_name) {
		/* check for syntax like user=domain\user */
		if(got_domain == 0)
			domain_name = check_for_domain(&user_name);
		strlcat(options,",user=",options_size);
		strlcat(options,user_name,options_size);
	}
	if(retry == 0) {
		if(domain_name) {
			/* extra length accounted for in option string above */
			strlcat(options,",domain=",options_size);
			strlcat(options,domain_name,options_size);
		}
	}

	strlcat(options,",ver=",options_size);
	strlcat(options,MOUNT_CIFS_VERSION_MAJOR,options_size);

	if(orgoptions) {
		strlcat(options,",",options_size);
		strlcat(options,orgoptions,options_size);
	}
	if(prefixpath) {
		strlcat(options,",prefixpath=",options_size);
		strlcat(options,prefixpath,options_size); /* no need to cat the / */
	}

	/* convert all '\\' to '/' in share portion so that /proc/mounts looks pretty */
	replace_char(dev_name, '\\', '/', strlen(share_name));

	if (!got_ip && addr) {
		strlcat(options, ",ip=", options_size);
		current_len = strnlen(options, options_size);
		optionstail = options + current_len;
		switch (addr->ai_addr->sa_family) {
		case AF_INET6:
			addr6 = (struct sockaddr_in6 *) addr->ai_addr;
			ipaddr = inet_ntop(AF_INET6, &addr6->sin6_addr, optionstail,
					   options_size - current_len);
			break;
		case AF_INET:
			addr4 = (struct sockaddr_in *) addr->ai_addr;
			ipaddr = inet_ntop(AF_INET, &addr4->sin_addr, optionstail,
					   options_size - current_len);
			break;
		default:
			ipaddr = NULL;
		}

		/* if the address looks bogus, try the next one */
		if (!ipaddr) {
			addr = addr->ai_next;
			if (addr)
				goto mount_retry;
			rc = EX_SYSERR;
			goto mount_exit;
		}
	}

	if (addr->ai_addr->sa_family == AF_INET6 && addr6->sin6_scope_id) {
		strlcat(options, "%", options_size);
		current_len = strnlen(options, options_size);
		optionstail = options + current_len;
		snprintf(optionstail, options_size - current_len, "%u",
			 addr6->sin6_scope_id);
	}

	if(verboseflag)
		fprintf(stderr, "\nmount.cifs kernel mount options: %s", options);

	if (mountpassword) {
		/*
		 * Commas have to be doubled, or else they will
		 * look like the parameter separator
		 */
		if(retry == 0)
			check_for_comma(&mountpassword);
		strlcat(options,",pass=",options_size);
		strlcat(options,mountpassword,options_size);
		if (verboseflag)
			fprintf(stderr, ",pass=********");
	}

	if (verboseflag)
		fprintf(stderr, "\n");

	if (!fakemnt && mount(dev_name, ".", cifs_fstype, flags, options)) {
		switch (errno) {
		case ECONNREFUSED:
		case EHOSTUNREACH:
			if (addr) {
				addr = addr->ai_next;
				if (addr)
					goto mount_retry;
			}
			break;
		case ENODEV:
			fprintf(stderr, "mount error: cifs filesystem not supported by the system\n");
			break;
		case ENXIO:
			if(retry == 0) {
				retry = 1;
				if (uppercase_string(dev_name) &&
				    uppercase_string(share_name) &&
				    uppercase_string(prefixpath)) {
					fprintf(stderr, "retrying with upper case share name\n");
					goto mount_retry;
				}
			}
		}
		fprintf(stderr, "mount error(%d): %s\n", errno, strerror(errno));
		fprintf(stderr, "Refer to the mount.cifs(8) manual page (e.g. man "
		       "mount.cifs)\n");
		rc = EX_FAIL;
		goto mount_exit;
	}

	if (nomtab)
		goto mount_exit;
	atexit(unlock_mtab);
	rc = lock_mtab();
	if (rc) {
		fprintf(stderr, "cannot lock mtab");
		goto mount_exit;
	}
	pmntfile = setmntent(MOUNTED, "a+");
	if (!pmntfile) {
		fprintf(stderr, "could not update mount table\n");
		unlock_mtab();
		rc = EX_FILEIO;
		goto mount_exit;
	}
	mountent.mnt_fsname = dev_name;
	mountent.mnt_dir = mountpoint;
	mountent.mnt_type = (char *)(void *)cifs_fstype;
	mountent.mnt_opts = (char *)malloc(220);
	if(mountent.mnt_opts) {
		char * mount_user = getusername();
		memset(mountent.mnt_opts,0,200);
		if(flags & MS_RDONLY)
			strlcat(mountent.mnt_opts,"ro",220);
		else
			strlcat(mountent.mnt_opts,"rw",220);
		if(flags & MS_MANDLOCK)
			strlcat(mountent.mnt_opts,",mand",220);
		if(flags & MS_NOEXEC)
			strlcat(mountent.mnt_opts,",noexec",220);
		if(flags & MS_NOSUID)
			strlcat(mountent.mnt_opts,",nosuid",220);
		if(flags & MS_NODEV)
			strlcat(mountent.mnt_opts,",nodev",220);
		if(flags & MS_SYNCHRONOUS)
			strlcat(mountent.mnt_opts,",sync",220);
		if(mount_user) {
			if(getuid() != 0) {
				strlcat(mountent.mnt_opts,
					",user=", 220);
				strlcat(mountent.mnt_opts,
					mount_user, 220);
			}
		}
	}
	mountent.mnt_freq = 0;
	mountent.mnt_passno = 0;
	rc = addmntent(pmntfile,&mountent);
	endmntent(pmntfile);
	unlock_mtab();
	SAFE_FREE(mountent.mnt_opts);
	if (rc)
		rc = EX_FILEIO;
mount_exit:
	if(mountpassword) {
		int len = strlen(mountpassword);
		memset(mountpassword,0,len);
		SAFE_FREE(mountpassword);
	}

	if (addrhead)
		freeaddrinfo(addrhead);
	SAFE_FREE(options);
	SAFE_FREE(orgoptions);
	SAFE_FREE(resolved_path);
	SAFE_FREE(share_name);
	exit(rc);
}