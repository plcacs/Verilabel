void csync_daemon_session()
{
	static char line[4 * 4096];
	struct stat sb;
	address_t peername = { .sa.sa_family = AF_UNSPEC, };
	socklen_t peerlen = sizeof(peername);
	char *peer=0, *tag[32];
	int i;


	if (fstat(0, &sb))
		csync_fatal("Can't run fstat on fd 0: %s", strerror(errno));

	switch (sb.st_mode & S_IFMT) {
	case S_IFSOCK:
		if ( getpeername(0, &peername.sa, &peerlen) == -1 )
			csync_fatal("Can't run getpeername on fd 0: %s", strerror(errno));
		break;
	case S_IFIFO:
		set_peername_from_env(&peername, "SSH_CLIENT");
		break;
		/* fall through */
	default:
		csync_fatal("I'm only talking to sockets or pipes! %x\n", sb.st_mode & S_IFMT);
		break;
	}

	while ( conn_gets(line, sizeof(line)) ) {
		int cmdnr;

		if (!setup_tag(tag, line))
			continue;

		for (cmdnr=0; cmdtab[cmdnr].text; cmdnr++)
			if ( !strcasecmp(cmdtab[cmdnr].text, tag[0]) ) break;

		if ( !cmdtab[cmdnr].text ) {
			cmd_error = conn_response(CR_ERR_UNKNOWN_COMMAND);
			goto abort_cmd;
		}

		cmd_error = 0;

		if ( cmdtab[cmdnr].need_ident && !peer ) {
			conn_printf("Dear %s, please identify first.\n",
				    csync_inet_ntop(&peername) ?: "stranger");
			goto next_cmd;
		}

		if ( cmdtab[cmdnr].check_perm )
			on_cygwin_lowercase(tag[2]);

		if ( cmdtab[cmdnr].check_perm ) {
			if ( cmdtab[cmdnr].check_perm == 2 )
				csync_compare_mode = 1;
			int perm = csync_perm(tag[2], tag[1], peer);
			if ( cmdtab[cmdnr].check_perm == 2 )
				csync_compare_mode = 0;
			if ( perm ) {
				if ( perm == 2 ) {
					csync_mark(tag[2], peer, 0);
					cmd_error = conn_response(CR_ERR_PERM_DENIED_FOR_SLAVE);
				} else
					cmd_error = conn_response(CR_ERR_PERM_DENIED);
				goto abort_cmd;
			}
		}

		if ( cmdtab[cmdnr].check_dirty &&
			csync_check_dirty(tag[2], peer, cmdtab[cmdnr].action == A_FLUSH) )
			goto abort_cmd;

		if ( cmdtab[cmdnr].unlink )
			csync_unlink(tag[2], cmdtab[cmdnr].unlink);

		switch ( cmdtab[cmdnr].action )
		{
		case A_SIG:
			{
				struct stat st;

				if ( lstat_strict(prefixsubst(tag[2]), &st) != 0 ) {
					if ( errno == ENOENT ) {
						struct stat sb;
						char parent_dirname[strlen(tag[2])];
						split_dirname_basename(parent_dirname, NULL, tag[2]);
						if ( lstat_strict(prefixsubst(parent_dirname), &sb) != 0 )
							cmd_error = conn_response(CR_ERR_PARENT_DIR_MISSING);
						else
							conn_resp_zero(CR_OK_PATH_NOT_FOUND);
					} else
						cmd_error = strerror(errno);
					break;
				} else
					if ( csync_check_pure(tag[2]) ) {
						conn_resp_zero(CR_OK_NOT_FOUND);
						break;
					}
				conn_resp(CR_OK_DATA_FOLLOWS);
				conn_printf("%s\n", csync_genchecktxt(&st, tag[2], 1));

				if ( S_ISREG(st.st_mode) )
					csync_rs_sig(tag[2]);
				else
					conn_printf("octet-stream 0\n");
			}
			break;
		case A_MARK:
			csync_mark(tag[2], peer, 0);
			break;
		case A_TYPE:
			{
				FILE *f = fopen(prefixsubst(tag[2]), "rb");

				if (!f && errno == ENOENT)
					f = fopen("/dev/null", "rb");

				if (f) {
					char buffer[512];
					size_t rc;

					conn_resp(CR_OK_DATA_FOLLOWS);
					while ( (rc=fread(buffer, 1, 512, f)) > 0 )
						if ( conn_write(buffer, rc) != rc ) {
							conn_printf("[[ %s ]]", strerror(errno));
							break;
						}
					fclose(f);
					return;
				}
				cmd_error = strerror(errno);
			}
			break;
		case A_GETTM:
		case A_GETSZ:
			{
				struct stat sbuf;
				conn_resp(CR_OK_DATA_FOLLOWS);
				if (!lstat_strict(prefixsubst(tag[2]), &sbuf))
					conn_printf("%ld\n", cmdtab[cmdnr].action == A_GETTM ?
							(long)sbuf.st_mtime : (long)sbuf.st_size);
				else
					conn_printf("-1\n");
				goto next_cmd;
			}
			break;
		case A_FLUSH:
			SQL("Flushing dirty entry (if any) for file",
				"DELETE FROM dirty WHERE filename = '%s'",
				url_encode(tag[2]));
			break;
		case A_DEL:
			if (!csync_file_backup(tag[2]))
				csync_unlink(tag[2], 0);
			break;
		case A_PATCH:
			if (!csync_file_backup(tag[2])) {
				conn_resp(CR_OK_SEND_DATA);
				csync_rs_sig(tag[2]);
				if (csync_rs_patch(tag[2]))
					cmd_error = strerror(errno);
			}
			break;
		case A_MKDIR:
			/* ignore errors on creating directories if the
			 * directory does exist already. we don't need such
			 * a check for the other file types, because they
			 * will simply be unlinked if already present.
			 */
#ifdef __CYGWIN__
			// This creates the file using the native windows API, bypassing
			// the cygwin wrappers and so making sure that we do not mess up the
			// permissions..
			{
				char winfilename[MAX_PATH];
				cygwin_conv_to_win32_path(prefixsubst(tag[2]), winfilename);

				if ( !CreateDirectory(TEXT(winfilename), NULL) ) {
					struct stat st;
					if ( lstat_strict(prefixsubst(tag[2]), &st) != 0 || !S_ISDIR(st.st_mode)) {
						csync_debug(1, "Win32 I/O Error %d in mkdir command: %s\n",
								(int)GetLastError(), winfilename);
						cmd_error = conn_response(CR_ERR_WIN32_EIO_CREATE_DIR);
					}
				}
			}
#else
			if ( mkdir(prefixsubst(tag[2]), 0700) ) {
				struct stat st;
				if ( lstat_strict((prefixsubst(tag[2])), &st) != 0 || !S_ISDIR(st.st_mode))
					cmd_error = strerror(errno);
			}
#endif
			break;
		case A_MKCHR:
			if ( mknod(prefixsubst(tag[2]), 0700|S_IFCHR, atoi(tag[3])) )
				cmd_error = strerror(errno);
			break;
		case A_MKBLK:
			if ( mknod(prefixsubst(tag[2]), 0700|S_IFBLK, atoi(tag[3])) )
				cmd_error = strerror(errno);
			break;
		case A_MKFIFO:
			if ( mknod(prefixsubst(tag[2]), 0700|S_IFIFO, 0) )
				cmd_error = strerror(errno);
			break;
		case A_MKLINK:
			if ( symlink(tag[3], prefixsubst(tag[2])) )
				cmd_error = strerror(errno);
			break;
		case A_MKSOCK:
			/* just ignore socket files */
			break;
		case A_SETOWN:
			if ( !csync_ignore_uid || !csync_ignore_gid ) {
				int uid = csync_ignore_uid ? -1 : atoi(tag[3]);
				int gid = csync_ignore_gid ? -1 : atoi(tag[4]);
				if ( lchown(prefixsubst(tag[2]), uid, gid) )
					cmd_error = strerror(errno);
			}
			break;
		case A_SETMOD:
			if ( !csync_ignore_mod ) {
				if ( chmod(prefixsubst(tag[2]), atoi(tag[3])) )
					cmd_error = strerror(errno);
			}
			break;
		case A_SETIME:
			{
				struct utimbuf utb;
				utb.actime = atoll(tag[3]);
				utb.modtime = atoll(tag[3]);
				if ( utime(prefixsubst(tag[2]), &utb) )
					cmd_error = strerror(errno);
			}
			break;
		case A_LIST:
			SQL_BEGIN("DB Dump - Files for sync pair",
				"SELECT checktxt, filename FROM file %s%s%s ORDER BY filename",
					strcmp(tag[2], "-") ? "WHERE filename = '" : "",
					strcmp(tag[2], "-") ? url_encode(tag[2]) : "",
					strcmp(tag[2], "-") ? "'" : "")
			{
				if ( csync_match_file_host(url_decode(SQL_V(1)), tag[1], peer, (const char **)&tag[3]) )
					conn_printf("%s\t%s\n", SQL_V(0), SQL_V(1));
			} SQL_END;
			break;

		case A_DEBUG:
			csync_debug_out = stdout;
			if ( tag[1][0] )
				csync_debug_level = atoi(tag[1]);
			break;
		case A_HELLO:
			if (peer) {
				free(peer);
				peer = NULL;
			}
			if (verify_peername(tag[1], &peername)) {
				peer = strdup(tag[1]);
			} else {
				peer = NULL;
				cmd_error = conn_response(CR_ERR_IDENTIFICATION_FAILED);
				break;
			}
#ifdef HAVE_LIBGNUTLS
			if (!csync_conn_usessl) {
				struct csync_nossl *t;
				for (t = csync_nossl; t; t=t->next) {
					if ( !fnmatch(t->pattern_from, myhostname, 0) &&
					     !fnmatch(t->pattern_to, peer, 0) )
						goto conn_without_ssl_ok;
				}
				cmd_error = conn_response(CR_ERR_SSL_EXPECTED);
			}
conn_without_ssl_ok:;
#endif
			break;
		case A_GROUP:
			if (active_grouplist) {
				cmd_error = conn_response(CR_ERR_GROUP_LIST_ALREADY_SET);
			} else {
				const struct csync_group *g;
				int i, gnamelen;

				active_grouplist = strdup(tag[1]);
				for (g=csync_group; g; g=g->next) {
					if (!g->myname) continue;

					i=0;
					gnamelen = strlen(csync_group->gname);
					while (active_grouplist[i])
					{
						if ( !strncmp(active_grouplist+i, csync_group->gname, gnamelen) &&
								(active_grouplist[i+gnamelen] == ',' ||
								 !active_grouplist[i+gnamelen]) )
							goto found_asactive;
						while (active_grouplist[i])
							if (active_grouplist[i++]==',') break;
					}
					csync_group->myname = 0;
found_asactive: ;
				}
			}
			break;
		case A_BYE:
			for (i=0; i<32; i++)
				free(tag[i]);
			conn_resp(CR_OK_CU_LATER);
			return;
		}

		if ( cmdtab[cmdnr].update )
			csync_file_update(tag[2], peer);

		if ( cmdtab[cmdnr].update == 1 ) {
			csync_debug(1, "Updated %s from %s.\n",
					tag[2], peer ? peer : "???");
			csync_schedule_commands(tag[2], 0);
		}

abort_cmd:
		if ( cmd_error )
			conn_printf("%s\n", cmd_error);
		else
			conn_resp(CR_OK_CMD_FINISHED);

next_cmd:
		destroy_tag(tag);
	}
}