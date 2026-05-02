PQconnectPoll(PGconn *conn)
{
	bool		reset_connection_state_machine = false;
	bool		need_new_connection = false;
	PGresult   *res;
	char		sebuf[PG_STRERROR_R_BUFLEN];
	int			optval;

	if (conn == NULL)
		return PGRES_POLLING_FAILED;

	/* Get the new data */
	switch (conn->status)
	{
			/*
			 * We really shouldn't have been polled in these two cases, but we
			 * can handle it.
			 */
		case CONNECTION_BAD:
			return PGRES_POLLING_FAILED;
		case CONNECTION_OK:
			return PGRES_POLLING_OK;

			/* These are reading states */
		case CONNECTION_AWAITING_RESPONSE:
		case CONNECTION_AUTH_OK:
		case CONNECTION_CHECK_WRITABLE:
		case CONNECTION_CONSUME:
		case CONNECTION_CHECK_STANDBY:
			{
				/* Load waiting data */
				int			n = pqReadData(conn);

				if (n < 0)
					goto error_return;
				if (n == 0)
					return PGRES_POLLING_READING;

				break;
			}

			/* These are writing states, so we just proceed. */
		case CONNECTION_STARTED:
		case CONNECTION_MADE:
			break;

			/* Special cases: proceed without waiting. */
		case CONNECTION_SSL_STARTUP:
		case CONNECTION_NEEDED:
		case CONNECTION_GSS_STARTUP:
		case CONNECTION_CHECK_TARGET:
			break;

		default:
			appendPQExpBufferStr(&conn->errorMessage,
								 libpq_gettext("invalid connection state, probably indicative of memory corruption\n"));
			goto error_return;
	}


keep_going:						/* We will come back to here until there is
								 * nothing left to do. */

	/* Time to advance to next address, or next host if no more addresses? */
	if (conn->try_next_addr)
	{
		if (conn->addr_cur && conn->addr_cur->ai_next)
		{
			conn->addr_cur = conn->addr_cur->ai_next;
			reset_connection_state_machine = true;
		}
		else
			conn->try_next_host = true;
		conn->try_next_addr = false;
	}

	/* Time to advance to next connhost[] entry? */
	if (conn->try_next_host)
	{
		pg_conn_host *ch;
		struct addrinfo hint;
		int			thisport;
		int			ret;
		char		portstr[MAXPGPATH];

		if (conn->whichhost + 1 < conn->nconnhost)
			conn->whichhost++;
		else
		{
			/*
			 * Oops, no more hosts.
			 *
			 * If we are trying to connect in "prefer-standby" mode, then drop
			 * the standby requirement and start over.
			 *
			 * Otherwise, an appropriate error message is already set up, so
			 * we just need to set the right status.
			 */
			if (conn->target_server_type == SERVER_TYPE_PREFER_STANDBY &&
				conn->nconnhost > 0)
			{
				conn->target_server_type = SERVER_TYPE_PREFER_STANDBY_PASS2;
				conn->whichhost = 0;
			}
			else
				goto error_return;
		}

		/* Drop any address info for previous host */
		release_conn_addrinfo(conn);

		/*
		 * Look up info for the new host.  On failure, log the problem in
		 * conn->errorMessage, then loop around to try the next host.  (Note
		 * we don't clear try_next_host until we've succeeded.)
		 */
		ch = &conn->connhost[conn->whichhost];

		/* Initialize hint structure */
		MemSet(&hint, 0, sizeof(hint));
		hint.ai_socktype = SOCK_STREAM;
		conn->addrlist_family = hint.ai_family = AF_UNSPEC;

		/* Figure out the port number we're going to use. */
		if (ch->port == NULL || ch->port[0] == '\0')
			thisport = DEF_PGPORT;
		else
		{
			if (!parse_int_param(ch->port, &thisport, conn, "port"))
				goto error_return;

			if (thisport < 1 || thisport > 65535)
			{
				appendPQExpBuffer(&conn->errorMessage,
								  libpq_gettext("invalid port number: \"%s\"\n"),
								  ch->port);
				goto keep_going;
			}
		}
		snprintf(portstr, sizeof(portstr), "%d", thisport);

		/* Use pg_getaddrinfo_all() to resolve the address */
		switch (ch->type)
		{
			case CHT_HOST_NAME:
				ret = pg_getaddrinfo_all(ch->host, portstr, &hint,
										 &conn->addrlist);
				if (ret || !conn->addrlist)
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("could not translate host name \"%s\" to address: %s\n"),
									  ch->host, gai_strerror(ret));
					goto keep_going;
				}
				break;

			case CHT_HOST_ADDRESS:
				hint.ai_flags = AI_NUMERICHOST;
				ret = pg_getaddrinfo_all(ch->hostaddr, portstr, &hint,
										 &conn->addrlist);
				if (ret || !conn->addrlist)
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("could not parse network address \"%s\": %s\n"),
									  ch->hostaddr, gai_strerror(ret));
					goto keep_going;
				}
				break;

			case CHT_UNIX_SOCKET:
#ifdef HAVE_UNIX_SOCKETS
				conn->addrlist_family = hint.ai_family = AF_UNIX;
				UNIXSOCK_PATH(portstr, thisport, ch->host);
				if (strlen(portstr) >= UNIXSOCK_PATH_BUFLEN)
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("Unix-domain socket path \"%s\" is too long (maximum %d bytes)\n"),
									  portstr,
									  (int) (UNIXSOCK_PATH_BUFLEN - 1));
					goto keep_going;
				}

				/*
				 * NULL hostname tells pg_getaddrinfo_all to parse the service
				 * name as a Unix-domain socket path.
				 */
				ret = pg_getaddrinfo_all(NULL, portstr, &hint,
										 &conn->addrlist);
				if (ret || !conn->addrlist)
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("could not translate Unix-domain socket path \"%s\" to address: %s\n"),
									  portstr, gai_strerror(ret));
					goto keep_going;
				}
#else
				Assert(false);
#endif
				break;
		}

		/* OK, scan this addrlist for a working server address */
		conn->addr_cur = conn->addrlist;
		reset_connection_state_machine = true;
		conn->try_next_host = false;
	}

	/* Reset connection state machine? */
	if (reset_connection_state_machine)
	{
		/*
		 * (Re) initialize our connection control variables for a set of
		 * connection attempts to a single server address.  These variables
		 * must persist across individual connection attempts, but we must
		 * reset them when we start to consider a new server.
		 */
		conn->pversion = PG_PROTOCOL(3, 0);
		conn->send_appname = true;
#ifdef USE_SSL
		/* initialize these values based on SSL mode */
		conn->allow_ssl_try = (conn->sslmode[0] != 'd');	/* "disable" */
		conn->wait_ssl_try = (conn->sslmode[0] == 'a'); /* "allow" */
#endif
#ifdef ENABLE_GSS
		conn->try_gss = (conn->gssencmode[0] != 'd');	/* "disable" */
#endif

		reset_connection_state_machine = false;
		need_new_connection = true;
	}

	/* Force a new connection (perhaps to the same server as before)? */
	if (need_new_connection)
	{
		/* Drop any existing connection */
		pqDropConnection(conn, true);

		/* Reset all state obtained from old server */
		pqDropServerData(conn);

		/* Drop any PGresult we might have, too */
		conn->asyncStatus = PGASYNC_IDLE;
		conn->xactStatus = PQTRANS_IDLE;
		conn->pipelineStatus = PQ_PIPELINE_OFF;
		pqClearAsyncResult(conn);

		/* Reset conn->status to put the state machine in the right state */
		conn->status = CONNECTION_NEEDED;

		need_new_connection = false;
	}

	/* Now try to advance the state machine for this connection */
	switch (conn->status)
	{
		case CONNECTION_NEEDED:
			{
				/*
				 * Try to initiate a connection to one of the addresses
				 * returned by pg_getaddrinfo_all().  conn->addr_cur is the
				 * next one to try.
				 *
				 * The extra level of braces here is historical.  It's not
				 * worth reindenting this whole switch case to remove 'em.
				 */
				{
					struct addrinfo *addr_cur = conn->addr_cur;
					char		host_addr[NI_MAXHOST];

					/*
					 * Advance to next possible host, if we've tried all of
					 * the addresses for the current host.
					 */
					if (addr_cur == NULL)
					{
						conn->try_next_host = true;
						goto keep_going;
					}

					/* Remember current address for possible use later */
					memcpy(&conn->raddr.addr, addr_cur->ai_addr,
						   addr_cur->ai_addrlen);
					conn->raddr.salen = addr_cur->ai_addrlen;

					/*
					 * Set connip, too.  Note we purposely ignore strdup
					 * failure; not a big problem if it fails.
					 */
					if (conn->connip != NULL)
					{
						free(conn->connip);
						conn->connip = NULL;
					}
					getHostaddr(conn, host_addr, NI_MAXHOST);
					if (host_addr[0])
						conn->connip = strdup(host_addr);

					/* Try to create the socket */
					conn->sock = socket(addr_cur->ai_family, SOCK_STREAM, 0);
					if (conn->sock == PGINVALID_SOCKET)
					{
						int			errorno = SOCK_ERRNO;

						/*
						 * Silently ignore socket() failure if we have more
						 * addresses to try; this reduces useless chatter in
						 * cases where the address list includes both IPv4 and
						 * IPv6 but kernel only accepts one family.
						 */
						if (addr_cur->ai_next != NULL ||
							conn->whichhost + 1 < conn->nconnhost)
						{
							conn->try_next_addr = true;
							goto keep_going;
						}
						emitHostIdentityInfo(conn, host_addr);
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("could not create socket: %s\n"),
										  SOCK_STRERROR(errorno, sebuf, sizeof(sebuf)));
						goto error_return;
					}

					/*
					 * Once we've identified a target address, all errors
					 * except the preceding socket()-failure case should be
					 * prefixed with host-identity information.  (If the
					 * connection succeeds, the contents of conn->errorMessage
					 * won't matter, so this is harmless.)
					 */
					emitHostIdentityInfo(conn, host_addr);

					/*
					 * Select socket options: no delay of outgoing data for
					 * TCP sockets, nonblock mode, close-on-exec.  Try the
					 * next address if any of this fails.
					 */
					if (!IS_AF_UNIX(addr_cur->ai_family))
					{
						if (!connectNoDelay(conn))
						{
							/* error message already created */
							conn->try_next_addr = true;
							goto keep_going;
						}
					}
					if (!pg_set_noblock(conn->sock))
					{
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("could not set socket to nonblocking mode: %s\n"),
										  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
						conn->try_next_addr = true;
						goto keep_going;
					}

#ifdef F_SETFD
					if (fcntl(conn->sock, F_SETFD, FD_CLOEXEC) == -1)
					{
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("could not set socket to close-on-exec mode: %s\n"),
										  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
						conn->try_next_addr = true;
						goto keep_going;
					}
#endif							/* F_SETFD */

					if (!IS_AF_UNIX(addr_cur->ai_family))
					{
#ifndef WIN32
						int			on = 1;
#endif
						int			usekeepalives = useKeepalives(conn);
						int			err = 0;

						if (usekeepalives < 0)
						{
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("keepalives parameter must be an integer\n"));
							err = 1;
						}
						else if (usekeepalives == 0)
						{
							/* Do nothing */
						}
#ifndef WIN32
						else if (setsockopt(conn->sock,
											SOL_SOCKET, SO_KEEPALIVE,
											(char *) &on, sizeof(on)) < 0)
						{
							appendPQExpBuffer(&conn->errorMessage,
											  libpq_gettext("%s(%s) failed: %s\n"),
											  "setsockopt",
											  "SO_KEEPALIVE",
											  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
							err = 1;
						}
						else if (!setKeepalivesIdle(conn)
								 || !setKeepalivesInterval(conn)
								 || !setKeepalivesCount(conn))
							err = 1;
#else							/* WIN32 */
#ifdef SIO_KEEPALIVE_VALS
						else if (!setKeepalivesWin32(conn))
							err = 1;
#endif							/* SIO_KEEPALIVE_VALS */
#endif							/* WIN32 */
						else if (!setTCPUserTimeout(conn))
							err = 1;

						if (err)
						{
							conn->try_next_addr = true;
							goto keep_going;
						}
					}

					/*----------
					 * We have three methods of blocking SIGPIPE during
					 * send() calls to this socket:
					 *
					 *	- setsockopt(sock, SO_NOSIGPIPE)
					 *	- send(sock, ..., MSG_NOSIGNAL)
					 *	- setting the signal mask to SIG_IGN during send()
					 *
					 * The third method requires three syscalls per send,
					 * so we prefer either of the first two, but they are
					 * less portable.  The state is tracked in the following
					 * members of PGconn:
					 *
					 * conn->sigpipe_so		- we have set up SO_NOSIGPIPE
					 * conn->sigpipe_flag	- we're specifying MSG_NOSIGNAL
					 *
					 * If we can use SO_NOSIGPIPE, then set sigpipe_so here
					 * and we're done.  Otherwise, set sigpipe_flag so that
					 * we will try MSG_NOSIGNAL on sends.  If we get an error
					 * with MSG_NOSIGNAL, we'll clear that flag and revert to
					 * signal masking.
					 *----------
					 */
					conn->sigpipe_so = false;
#ifdef MSG_NOSIGNAL
					conn->sigpipe_flag = true;
#else
					conn->sigpipe_flag = false;
#endif							/* MSG_NOSIGNAL */

#ifdef SO_NOSIGPIPE
					optval = 1;
					if (setsockopt(conn->sock, SOL_SOCKET, SO_NOSIGPIPE,
								   (char *) &optval, sizeof(optval)) == 0)
					{
						conn->sigpipe_so = true;
						conn->sigpipe_flag = false;
					}
#endif							/* SO_NOSIGPIPE */

					/*
					 * Start/make connection.  This should not block, since we
					 * are in nonblock mode.  If it does, well, too bad.
					 */
					if (connect(conn->sock, addr_cur->ai_addr,
								addr_cur->ai_addrlen) < 0)
					{
						if (SOCK_ERRNO == EINPROGRESS ||
#ifdef WIN32
							SOCK_ERRNO == EWOULDBLOCK ||
#endif
							SOCK_ERRNO == EINTR)
						{
							/*
							 * This is fine - we're in non-blocking mode, and
							 * the connection is in progress.  Tell caller to
							 * wait for write-ready on socket.
							 */
							conn->status = CONNECTION_STARTED;
							return PGRES_POLLING_WRITING;
						}
						/* otherwise, trouble */
					}
					else
					{
						/*
						 * Hm, we're connected already --- seems the "nonblock
						 * connection" wasn't.  Advance the state machine and
						 * go do the next stuff.
						 */
						conn->status = CONNECTION_STARTED;
						goto keep_going;
					}

					/*
					 * This connection failed.  Add the error report to
					 * conn->errorMessage, then try the next address if any.
					 */
					connectFailureMessage(conn, SOCK_ERRNO);
					conn->try_next_addr = true;
					goto keep_going;
				}
			}

		case CONNECTION_STARTED:
			{
				ACCEPT_TYPE_ARG3 optlen = sizeof(optval);

				/*
				 * Write ready, since we've made it here, so the connection
				 * has been made ... or has failed.
				 */

				/*
				 * Now check (using getsockopt) that there is not an error
				 * state waiting for us on the socket.
				 */

				if (getsockopt(conn->sock, SOL_SOCKET, SO_ERROR,
							   (char *) &optval, &optlen) == -1)
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("could not get socket error status: %s\n"),
									  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
					goto error_return;
				}
				else if (optval != 0)
				{
					/*
					 * When using a nonblocking connect, we will typically see
					 * connect failures at this point, so provide a friendly
					 * error message.
					 */
					connectFailureMessage(conn, optval);

					/*
					 * Try the next address if any, just as in the case where
					 * connect() returned failure immediately.
					 */
					conn->try_next_addr = true;
					goto keep_going;
				}

				/* Fill in the client address */
				conn->laddr.salen = sizeof(conn->laddr.addr);
				if (getsockname(conn->sock,
								(struct sockaddr *) &conn->laddr.addr,
								&conn->laddr.salen) < 0)
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("could not get client address from socket: %s\n"),
									  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
					goto error_return;
				}

				/*
				 * Make sure we can write before advancing to next step.
				 */
				conn->status = CONNECTION_MADE;
				return PGRES_POLLING_WRITING;
			}

		case CONNECTION_MADE:
			{
				char	   *startpacket;
				int			packetlen;

				/*
				 * Implement requirepeer check, if requested and it's a
				 * Unix-domain socket.
				 */
				if (conn->requirepeer && conn->requirepeer[0] &&
					IS_AF_UNIX(conn->raddr.addr.ss_family))
				{
#ifndef WIN32
					char		pwdbuf[BUFSIZ];
					struct passwd pass_buf;
					struct passwd *pass;
					int			passerr;
#endif
					uid_t		uid;
					gid_t		gid;

					errno = 0;
					if (getpeereid(conn->sock, &uid, &gid) != 0)
					{
						/*
						 * Provide special error message if getpeereid is a
						 * stub
						 */
						if (errno == ENOSYS)
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("requirepeer parameter is not supported on this platform\n"));
						else
							appendPQExpBuffer(&conn->errorMessage,
											  libpq_gettext("could not get peer credentials: %s\n"),
											  strerror_r(errno, sebuf, sizeof(sebuf)));
						goto error_return;
					}

#ifndef WIN32
					passerr = pqGetpwuid(uid, &pass_buf, pwdbuf, sizeof(pwdbuf), &pass);
					if (pass == NULL)
					{
						if (passerr != 0)
							appendPQExpBuffer(&conn->errorMessage,
											  libpq_gettext("could not look up local user ID %d: %s\n"),
											  (int) uid,
											  strerror_r(passerr, sebuf, sizeof(sebuf)));
						else
							appendPQExpBuffer(&conn->errorMessage,
											  libpq_gettext("local user with ID %d does not exist\n"),
											  (int) uid);
						goto error_return;
					}

					if (strcmp(pass->pw_name, conn->requirepeer) != 0)
					{
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("requirepeer specifies \"%s\", but actual peer user name is \"%s\"\n"),
										  conn->requirepeer, pass->pw_name);
						goto error_return;
					}
#else							/* WIN32 */
					/* should have failed with ENOSYS above */
					Assert(false);
#endif							/* WIN32 */
				}

				if (IS_AF_UNIX(conn->raddr.addr.ss_family))
				{
					/* Don't request SSL or GSSAPI over Unix sockets */
#ifdef USE_SSL
					conn->allow_ssl_try = false;
#endif
#ifdef ENABLE_GSS
					conn->try_gss = false;
#endif
				}

#ifdef ENABLE_GSS

				/*
				 * If GSSAPI encryption is enabled, then call
				 * pg_GSS_have_cred_cache() which will return true if we can
				 * acquire credentials (and give us a handle to use in
				 * conn->gcred), and then send a packet to the server asking
				 * for GSSAPI Encryption (and skip past SSL negotiation and
				 * regular startup below).
				 */
				if (conn->try_gss && !conn->gctx)
					conn->try_gss = pg_GSS_have_cred_cache(&conn->gcred);
				if (conn->try_gss && !conn->gctx)
				{
					ProtocolVersion pv = pg_hton32(NEGOTIATE_GSS_CODE);

					if (pqPacketSend(conn, 0, &pv, sizeof(pv)) != STATUS_OK)
					{
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("could not send GSSAPI negotiation packet: %s\n"),
										  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
						goto error_return;
					}

					/* Ok, wait for response */
					conn->status = CONNECTION_GSS_STARTUP;
					return PGRES_POLLING_READING;
				}
				else if (!conn->gctx && conn->gssencmode[0] == 'r')
				{
					appendPQExpBufferStr(&conn->errorMessage,
										 libpq_gettext("GSSAPI encryption required but was impossible (possibly no credential cache, no server support, or using a local socket)\n"));
					goto error_return;
				}
#endif

#ifdef USE_SSL

				/*
				 * Enable the libcrypto callbacks before checking if SSL needs
				 * to be done.  This is done before sending the startup packet
				 * as depending on the type of authentication done, like MD5
				 * or SCRAM that use cryptohashes, the callbacks would be
				 * required even without a SSL connection
				 */
				if (pqsecure_initialize(conn, false, true) < 0)
					goto error_return;

				/*
				 * If SSL is enabled and we haven't already got encryption of
				 * some sort running, request SSL instead of sending the
				 * startup message.
				 */
				if (conn->allow_ssl_try && !conn->wait_ssl_try &&
					!conn->ssl_in_use
#ifdef ENABLE_GSS
					&& !conn->gssenc
#endif
					)
				{
					ProtocolVersion pv;

					/*
					 * Send the SSL request packet.
					 *
					 * Theoretically, this could block, but it really
					 * shouldn't since we only got here if the socket is
					 * write-ready.
					 */
					pv = pg_hton32(NEGOTIATE_SSL_CODE);
					if (pqPacketSend(conn, 0, &pv, sizeof(pv)) != STATUS_OK)
					{
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("could not send SSL negotiation packet: %s\n"),
										  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
						goto error_return;
					}
					/* Ok, wait for response */
					conn->status = CONNECTION_SSL_STARTUP;
					return PGRES_POLLING_READING;
				}
#endif							/* USE_SSL */

				/*
				 * Build the startup packet.
				 */
				startpacket = pqBuildStartupPacket3(conn, &packetlen,
													EnvironmentOptions);
				if (!startpacket)
				{
					appendPQExpBufferStr(&conn->errorMessage,
										 libpq_gettext("out of memory\n"));
					goto error_return;
				}

				/*
				 * Send the startup packet.
				 *
				 * Theoretically, this could block, but it really shouldn't
				 * since we only got here if the socket is write-ready.
				 */
				if (pqPacketSend(conn, 0, startpacket, packetlen) != STATUS_OK)
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("could not send startup packet: %s\n"),
									  SOCK_STRERROR(SOCK_ERRNO, sebuf, sizeof(sebuf)));
					free(startpacket);
					goto error_return;
				}

				free(startpacket);

				conn->status = CONNECTION_AWAITING_RESPONSE;
				return PGRES_POLLING_READING;
			}

			/*
			 * Handle SSL negotiation: wait for postmaster messages and
			 * respond as necessary.
			 */
		case CONNECTION_SSL_STARTUP:
			{
#ifdef USE_SSL
				PostgresPollingStatusType pollres;

				/*
				 * On first time through, get the postmaster's response to our
				 * SSL negotiation packet.
				 */
				if (!conn->ssl_in_use)
				{
					/*
					 * We use pqReadData here since it has the logic to
					 * distinguish no-data-yet from connection closure. Since
					 * conn->ssl isn't set, a plain recv() will occur.
					 */
					char		SSLok;
					int			rdresult;

					rdresult = pqReadData(conn);
					if (rdresult < 0)
					{
						/* errorMessage is already filled in */
						goto error_return;
					}
					if (rdresult == 0)
					{
						/* caller failed to wait for data */
						return PGRES_POLLING_READING;
					}
					if (pqGetc(&SSLok, conn) < 0)
					{
						/* should not happen really */
						return PGRES_POLLING_READING;
					}
					if (SSLok == 'S')
					{
						/* mark byte consumed */
						conn->inStart = conn->inCursor;

						/*
						 * Set up global SSL state if required.  The crypto
						 * state has already been set if libpq took care of
						 * doing that, so there is no need to make that happen
						 * again.
						 */
						if (pqsecure_initialize(conn, true, false) != 0)
							goto error_return;
					}
					else if (SSLok == 'N')
					{
						/* mark byte consumed */
						conn->inStart = conn->inCursor;
						/* OK to do without SSL? */
						if (conn->sslmode[0] == 'r' ||	/* "require" */
							conn->sslmode[0] == 'v')	/* "verify-ca" or
														 * "verify-full" */
						{
							/* Require SSL, but server does not want it */
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("server does not support SSL, but SSL was required\n"));
							goto error_return;
						}
						/* Otherwise, proceed with normal startup */
						conn->allow_ssl_try = false;
						/* We can proceed using this connection */
						conn->status = CONNECTION_MADE;
						return PGRES_POLLING_WRITING;
					}
					else if (SSLok == 'E')
					{
						/*
						 * Server failure of some sort, such as failure to
						 * fork a backend process.  We need to process and
						 * report the error message, which might be formatted
						 * according to either protocol 2 or protocol 3.
						 * Rather than duplicate the code for that, we flip
						 * into AWAITING_RESPONSE state and let the code there
						 * deal with it.  Note we have *not* consumed the "E"
						 * byte here.
						 */
						conn->status = CONNECTION_AWAITING_RESPONSE;
						goto keep_going;
					}
					else
					{
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("received invalid response to SSL negotiation: %c\n"),
										  SSLok);
						goto error_return;
					}
				}

				/*
				 * Begin or continue the SSL negotiation process.
				 */
				pollres = pqsecure_open_client(conn);
				if (pollres == PGRES_POLLING_OK)
				{
					/*
					 * At this point we should have no data already buffered.
					 * If we do, it was received before we performed the SSL
					 * handshake, so it wasn't encrypted and indeed may have
					 * been injected by a man-in-the-middle.
					 */
					if (conn->inCursor != conn->inEnd)
					{
						appendPQExpBufferStr(&conn->errorMessage,
											 libpq_gettext("received unencrypted data after SSL response\n"));
						goto error_return;
					}

					/* SSL handshake done, ready to send startup packet */
					conn->status = CONNECTION_MADE;
					return PGRES_POLLING_WRITING;
				}
				if (pollres == PGRES_POLLING_FAILED)
				{
					/*
					 * Failed ... if sslmode is "prefer" then do a non-SSL
					 * retry
					 */
					if (conn->sslmode[0] == 'p' /* "prefer" */
						&& conn->allow_ssl_try	/* redundant? */
						&& !conn->wait_ssl_try) /* redundant? */
					{
						/* only retry once */
						conn->allow_ssl_try = false;
						need_new_connection = true;
						goto keep_going;
					}
					/* Else it's a hard failure */
					goto error_return;
				}
				/* Else, return POLLING_READING or POLLING_WRITING status */
				return pollres;
#else							/* !USE_SSL */
				/* can't get here */
				goto error_return;
#endif							/* USE_SSL */
			}

		case CONNECTION_GSS_STARTUP:
			{
#ifdef ENABLE_GSS
				PostgresPollingStatusType pollres;

				/*
				 * If we haven't yet, get the postmaster's response to our
				 * negotiation packet
				 */
				if (conn->try_gss && !conn->gctx)
				{
					char		gss_ok;
					int			rdresult = pqReadData(conn);

					if (rdresult < 0)
						/* pqReadData fills in error message */
						goto error_return;
					else if (rdresult == 0)
						/* caller failed to wait for data */
						return PGRES_POLLING_READING;
					if (pqGetc(&gss_ok, conn) < 0)
						/* shouldn't happen... */
						return PGRES_POLLING_READING;

					if (gss_ok == 'E')
					{
						/*
						 * Server failure of some sort.  Assume it's a
						 * protocol version support failure, and let's see if
						 * we can't recover (if it's not, we'll get a better
						 * error message on retry).  Server gets fussy if we
						 * don't hang up the socket, though.
						 */
						conn->try_gss = false;
						need_new_connection = true;
						goto keep_going;
					}

					/* mark byte consumed */
					conn->inStart = conn->inCursor;

					if (gss_ok == 'N')
					{
						/* Server doesn't want GSSAPI; fall back if we can */
						if (conn->gssencmode[0] == 'r')
						{
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("server doesn't support GSSAPI encryption, but it was required\n"));
							goto error_return;
						}

						conn->try_gss = false;
						/* We can proceed using this connection */
						conn->status = CONNECTION_MADE;
						return PGRES_POLLING_WRITING;
					}
					else if (gss_ok != 'G')
					{
						appendPQExpBuffer(&conn->errorMessage,
										  libpq_gettext("received invalid response to GSSAPI negotiation: %c\n"),
										  gss_ok);
						goto error_return;
					}
				}

				/* Begin or continue GSSAPI negotiation */
				pollres = pqsecure_open_gss(conn);
				if (pollres == PGRES_POLLING_OK)
				{
					/*
					 * At this point we should have no data already buffered.
					 * If we do, it was received before we performed the GSS
					 * handshake, so it wasn't encrypted and indeed may have
					 * been injected by a man-in-the-middle.
					 */
					if (conn->inCursor != conn->inEnd)
					{
						appendPQExpBufferStr(&conn->errorMessage,
											 libpq_gettext("received unencrypted data after GSSAPI encryption response\n"));
						goto error_return;
					}

					/* All set for startup packet */
					conn->status = CONNECTION_MADE;
					return PGRES_POLLING_WRITING;
				}
				else if (pollres == PGRES_POLLING_FAILED &&
						 conn->gssencmode[0] == 'p')
				{
					/*
					 * We failed, but we can retry on "prefer".  Have to drop
					 * the current connection to do so, though.
					 */
					conn->try_gss = false;
					need_new_connection = true;
					goto keep_going;
				}
				return pollres;
#else							/* !ENABLE_GSS */
				/* unreachable */
				goto error_return;
#endif							/* ENABLE_GSS */
			}

			/*
			 * Handle authentication exchange: wait for postmaster messages
			 * and respond as necessary.
			 */
		case CONNECTION_AWAITING_RESPONSE:
			{
				char		beresp;
				int			msgLength;
				int			avail;
				AuthRequest areq;
				int			res;

				/*
				 * Scan the message from current point (note that if we find
				 * the message is incomplete, we will return without advancing
				 * inStart, and resume here next time).
				 */
				conn->inCursor = conn->inStart;

				/* Read type byte */
				if (pqGetc(&beresp, conn))
				{
					/* We'll come back when there is more data */
					return PGRES_POLLING_READING;
				}

				/*
				 * Validate message type: we expect only an authentication
				 * request or an error here.  Anything else probably means
				 * it's not Postgres on the other end at all.
				 */
				if (!(beresp == 'R' || beresp == 'E'))
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("expected authentication request from server, but received %c\n"),
									  beresp);
					goto error_return;
				}

				/* Read message length word */
				if (pqGetInt(&msgLength, 4, conn))
				{
					/* We'll come back when there is more data */
					return PGRES_POLLING_READING;
				}

				/*
				 * Try to validate message length before using it.
				 * Authentication requests can't be very large, although GSS
				 * auth requests may not be that small.  Errors can be a
				 * little larger, but not huge.  If we see a large apparent
				 * length in an error, it means we're really talking to a
				 * pre-3.0-protocol server; cope.  (Before version 14, the
				 * server also used the old protocol for errors that happened
				 * before processing the startup packet.)
				 */
				if (beresp == 'R' && (msgLength < 8 || msgLength > 2000))
				{
					appendPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("expected authentication request from server, but received %c\n"),
									  beresp);
					goto error_return;
				}

				if (beresp == 'E' && (msgLength < 8 || msgLength > 30000))
				{
					/* Handle error from a pre-3.0 server */
					conn->inCursor = conn->inStart + 1; /* reread data */
					if (pqGets_append(&conn->errorMessage, conn))
					{
						/* We'll come back when there is more data */
						return PGRES_POLLING_READING;
					}
					/* OK, we read the message; mark data consumed */
					conn->inStart = conn->inCursor;

					/*
					 * Before 7.2, the postmaster didn't always end its
					 * messages with a newline, so add one if needed to
					 * conform to libpq conventions.
					 */
					if (conn->errorMessage.len == 0 ||
						conn->errorMessage.data[conn->errorMessage.len - 1] != '\n')
					{
						appendPQExpBufferChar(&conn->errorMessage, '\n');
					}

					goto error_return;
				}

				/*
				 * Can't process if message body isn't all here yet.
				 */
				msgLength -= 4;
				avail = conn->inEnd - conn->inCursor;
				if (avail < msgLength)
				{
					/*
					 * Before returning, try to enlarge the input buffer if
					 * needed to hold the whole message; see notes in
					 * pqParseInput3.
					 */
					if (pqCheckInBufferSpace(conn->inCursor + (size_t) msgLength,
											 conn))
						goto error_return;
					/* We'll come back when there is more data */
					return PGRES_POLLING_READING;
				}

				/* Handle errors. */
				if (beresp == 'E')
				{
					if (pqGetErrorNotice3(conn, true))
					{
						/* We'll come back when there is more data */
						return PGRES_POLLING_READING;
					}
					/* OK, we read the message; mark data consumed */
					conn->inStart = conn->inCursor;

					/*
					 * If error is "cannot connect now", try the next host if
					 * any (but we don't want to consider additional addresses
					 * for this host, nor is there much point in changing SSL
					 * or GSS mode).  This is helpful when dealing with
					 * standby servers that might not be in hot-standby state.
					 */
					if (strcmp(conn->last_sqlstate,
							   ERRCODE_CANNOT_CONNECT_NOW) == 0)
					{
						conn->try_next_host = true;
						goto keep_going;
					}

					/* Check to see if we should mention pgpassfile */
					pgpassfileWarning(conn);

#ifdef ENABLE_GSS

					/*
					 * If gssencmode is "prefer" and we're using GSSAPI, retry
					 * without it.
					 */
					if (conn->gssenc && conn->gssencmode[0] == 'p')
					{
						/* only retry once */
						conn->try_gss = false;
						need_new_connection = true;
						goto keep_going;
					}
#endif

#ifdef USE_SSL

					/*
					 * if sslmode is "allow" and we haven't tried an SSL
					 * connection already, then retry with an SSL connection
					 */
					if (conn->sslmode[0] == 'a' /* "allow" */
						&& !conn->ssl_in_use
						&& conn->allow_ssl_try
						&& conn->wait_ssl_try)
					{
						/* only retry once */
						conn->wait_ssl_try = false;
						need_new_connection = true;
						goto keep_going;
					}

					/*
					 * if sslmode is "prefer" and we're in an SSL connection,
					 * then do a non-SSL retry
					 */
					if (conn->sslmode[0] == 'p' /* "prefer" */
						&& conn->ssl_in_use
						&& conn->allow_ssl_try	/* redundant? */
						&& !conn->wait_ssl_try) /* redundant? */
					{
						/* only retry once */
						conn->allow_ssl_try = false;
						need_new_connection = true;
						goto keep_going;
					}
#endif

					goto error_return;
				}

				/* It is an authentication request. */
				conn->auth_req_received = true;

				/* Get the type of request. */
				if (pqGetInt((int *) &areq, 4, conn))
				{
					/* We'll come back when there are more data */
					return PGRES_POLLING_READING;
				}
				msgLength -= 4;

				/*
				 * Process the rest of the authentication request message, and
				 * respond to it if necessary.
				 *
				 * Note that conn->pghost must be non-NULL if we are going to
				 * avoid the Kerberos code doing a hostname look-up.
				 */
				res = pg_fe_sendauth(areq, msgLength, conn);

				/* OK, we have processed the message; mark data consumed */
				conn->inStart = conn->inCursor;

				if (res != STATUS_OK)
					goto error_return;

				/*
				 * Just make sure that any data sent by pg_fe_sendauth is
				 * flushed out.  Although this theoretically could block, it
				 * really shouldn't since we don't send large auth responses.
				 */
				if (pqFlush(conn))
					goto error_return;

				if (areq == AUTH_REQ_OK)
				{
					/* We are done with authentication exchange */
					conn->status = CONNECTION_AUTH_OK;

					/*
					 * Set asyncStatus so that PQgetResult will think that
					 * what comes back next is the result of a query.  See
					 * below.
					 */
					conn->asyncStatus = PGASYNC_BUSY;
				}

				/* Look to see if we have more data yet. */
				goto keep_going;
			}

		case CONNECTION_AUTH_OK:
			{
				/*
				 * Now we expect to hear from the backend. A ReadyForQuery
				 * message indicates that startup is successful, but we might
				 * also get an Error message indicating failure. (Notice
				 * messages indicating nonfatal warnings are also allowed by
				 * the protocol, as are ParameterStatus and BackendKeyData
				 * messages.) Easiest way to handle this is to let
				 * PQgetResult() read the messages. We just have to fake it
				 * out about the state of the connection, by setting
				 * asyncStatus = PGASYNC_BUSY (done above).
				 */

				if (PQisBusy(conn))
					return PGRES_POLLING_READING;

				res = PQgetResult(conn);

				/*
				 * NULL return indicating we have gone to IDLE state is
				 * expected
				 */
				if (res)
				{
					if (res->resultStatus != PGRES_FATAL_ERROR)
						appendPQExpBufferStr(&conn->errorMessage,
											 libpq_gettext("unexpected message from server during startup\n"));
					else if (conn->send_appname &&
							 (conn->appname || conn->fbappname))
					{
						/*
						 * If we tried to send application_name, check to see
						 * if the error is about that --- pre-9.0 servers will
						 * reject it at this stage of the process.  If so,
						 * close the connection and retry without sending
						 * application_name.  We could possibly get a false
						 * SQLSTATE match here and retry uselessly, but there
						 * seems no great harm in that; we'll just get the
						 * same error again if it's unrelated.
						 */
						const char *sqlstate;

						sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE);
						if (sqlstate &&
							strcmp(sqlstate, ERRCODE_APPNAME_UNKNOWN) == 0)
						{
							PQclear(res);
							conn->send_appname = false;
							need_new_connection = true;
							goto keep_going;
						}
					}

					/*
					 * if the resultStatus is FATAL, then conn->errorMessage
					 * already has a copy of the error; needn't copy it back.
					 * But add a newline if it's not there already, since
					 * postmaster error messages may not have one.
					 */
					if (conn->errorMessage.len <= 0 ||
						conn->errorMessage.data[conn->errorMessage.len - 1] != '\n')
						appendPQExpBufferChar(&conn->errorMessage, '\n');
					PQclear(res);
					goto error_return;
				}

				/* Almost there now ... */
				conn->status = CONNECTION_CHECK_TARGET;
				goto keep_going;
			}

		case CONNECTION_CHECK_TARGET:
			{
				/*
				 * If a read-write, read-only, primary, or standby connection
				 * is required, see if we have one.
				 */
				if (conn->target_server_type == SERVER_TYPE_READ_WRITE ||
					conn->target_server_type == SERVER_TYPE_READ_ONLY)
				{
					bool		read_only_server;

					/*
					 * If the server didn't report
					 * "default_transaction_read_only" or "in_hot_standby" at
					 * startup, we must determine its state by sending the
					 * query "SHOW transaction_read_only".  This GUC exists in
					 * all server versions that support 3.0 protocol.
					 */
					if (conn->default_transaction_read_only == PG_BOOL_UNKNOWN ||
						conn->in_hot_standby == PG_BOOL_UNKNOWN)
					{
						/*
						 * We use PQsendQueryContinue so that
						 * conn->errorMessage does not get cleared.  We need
						 * to preserve any error messages related to previous
						 * hosts we have tried and failed to connect to.
						 */
						conn->status = CONNECTION_OK;
						if (!PQsendQueryContinue(conn,
												 "SHOW transaction_read_only"))
							goto error_return;
						/* We'll return to this state when we have the answer */
						conn->status = CONNECTION_CHECK_WRITABLE;
						return PGRES_POLLING_READING;
					}

					/* OK, we can make the test */
					read_only_server =
						(conn->default_transaction_read_only == PG_BOOL_YES ||
						 conn->in_hot_standby == PG_BOOL_YES);

					if ((conn->target_server_type == SERVER_TYPE_READ_WRITE) ?
						read_only_server : !read_only_server)
					{
						/* Wrong server state, reject and try the next host */
						if (conn->target_server_type == SERVER_TYPE_READ_WRITE)
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("session is read-only\n"));
						else
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("session is not read-only\n"));

						/* Close connection politely. */
						conn->status = CONNECTION_OK;
						sendTerminateConn(conn);

						/*
						 * Try next host if any, but we don't want to consider
						 * additional addresses for this host.
						 */
						conn->try_next_host = true;
						goto keep_going;
					}
				}
				else if (conn->target_server_type == SERVER_TYPE_PRIMARY ||
						 conn->target_server_type == SERVER_TYPE_STANDBY ||
						 conn->target_server_type == SERVER_TYPE_PREFER_STANDBY)
				{
					/*
					 * If the server didn't report "in_hot_standby" at
					 * startup, we must determine its state by sending the
					 * query "SELECT pg_catalog.pg_is_in_recovery()".  Servers
					 * before 9.0 don't have that function, but by the same
					 * token they don't have any standby mode, so we may just
					 * assume the result.
					 */
					if (conn->sversion < 90000)
						conn->in_hot_standby = PG_BOOL_NO;

					if (conn->in_hot_standby == PG_BOOL_UNKNOWN)
					{
						/*
						 * We use PQsendQueryContinue so that
						 * conn->errorMessage does not get cleared.  We need
						 * to preserve any error messages related to previous
						 * hosts we have tried and failed to connect to.
						 */
						conn->status = CONNECTION_OK;
						if (!PQsendQueryContinue(conn,
												 "SELECT pg_catalog.pg_is_in_recovery()"))
							goto error_return;
						/* We'll return to this state when we have the answer */
						conn->status = CONNECTION_CHECK_STANDBY;
						return PGRES_POLLING_READING;
					}

					/* OK, we can make the test */
					if ((conn->target_server_type == SERVER_TYPE_PRIMARY) ?
						(conn->in_hot_standby == PG_BOOL_YES) :
						(conn->in_hot_standby == PG_BOOL_NO))
					{
						/* Wrong server state, reject and try the next host */
						if (conn->target_server_type == SERVER_TYPE_PRIMARY)
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("server is in hot standby mode\n"));
						else
							appendPQExpBufferStr(&conn->errorMessage,
												 libpq_gettext("server is not in hot standby mode\n"));

						/* Close connection politely. */
						conn->status = CONNECTION_OK;
						sendTerminateConn(conn);

						/*
						 * Try next host if any, but we don't want to consider
						 * additional addresses for this host.
						 */
						conn->try_next_host = true;
						goto keep_going;
					}
				}

				/* We can release the address list now. */
				release_conn_addrinfo(conn);

				/*
				 * Contents of conn->errorMessage are no longer interesting
				 * (and it seems some clients expect it to be empty after a
				 * successful connection).
				 */
				resetPQExpBuffer(&conn->errorMessage);

				/* We are open for business! */
				conn->status = CONNECTION_OK;
				return PGRES_POLLING_OK;
			}

		case CONNECTION_CONSUME:
			{
				/*
				 * This state just makes sure the connection is idle after
				 * we've obtained the result of a SHOW or SELECT query.  Once
				 * we're clear, return to CONNECTION_CHECK_TARGET state to
				 * decide what to do next.  We must transiently set status =
				 * CONNECTION_OK in order to use the result-consuming
				 * subroutines.
				 */
				conn->status = CONNECTION_OK;
				if (!PQconsumeInput(conn))
					goto error_return;

				if (PQisBusy(conn))
				{
					conn->status = CONNECTION_CONSUME;
					return PGRES_POLLING_READING;
				}

				/* Call PQgetResult() again until we get a NULL result */
				res = PQgetResult(conn);
				if (res != NULL)
				{
					PQclear(res);
					conn->status = CONNECTION_CONSUME;
					return PGRES_POLLING_READING;
				}

				conn->status = CONNECTION_CHECK_TARGET;
				goto keep_going;
			}

		case CONNECTION_CHECK_WRITABLE:
			{
				/*
				 * Waiting for result of "SHOW transaction_read_only".  We
				 * must transiently set status = CONNECTION_OK in order to use
				 * the result-consuming subroutines.
				 */
				conn->status = CONNECTION_OK;
				if (!PQconsumeInput(conn))
					goto error_return;

				if (PQisBusy(conn))
				{
					conn->status = CONNECTION_CHECK_WRITABLE;
					return PGRES_POLLING_READING;
				}

				res = PQgetResult(conn);
				if (res && PQresultStatus(res) == PGRES_TUPLES_OK &&
					PQntuples(res) == 1)
				{
					char	   *val = PQgetvalue(res, 0, 0);

					/*
					 * "transaction_read_only = on" proves that at least one
					 * of default_transaction_read_only and in_hot_standby is
					 * on, but we don't actually know which.  We don't care
					 * though for the purpose of identifying a read-only
					 * session, so satisfy the CONNECTION_CHECK_TARGET code by
					 * claiming they are both on.  On the other hand, if it's
					 * a read-write session, they are certainly both off.
					 */
					if (strncmp(val, "on", 2) == 0)
					{
						conn->default_transaction_read_only = PG_BOOL_YES;
						conn->in_hot_standby = PG_BOOL_YES;
					}
					else
					{
						conn->default_transaction_read_only = PG_BOOL_NO;
						conn->in_hot_standby = PG_BOOL_NO;
					}
					PQclear(res);

					/* Finish reading messages before continuing */
					conn->status = CONNECTION_CONSUME;
					goto keep_going;
				}

				/* Something went wrong with "SHOW transaction_read_only". */
				if (res)
					PQclear(res);

				/* Append error report to conn->errorMessage. */
				appendPQExpBuffer(&conn->errorMessage,
								  libpq_gettext("\"%s\" failed\n"),
								  "SHOW transaction_read_only");

				/* Close connection politely. */
				conn->status = CONNECTION_OK;
				sendTerminateConn(conn);

				/* Try next host. */
				conn->try_next_host = true;
				goto keep_going;
			}

		case CONNECTION_CHECK_STANDBY:
			{
				/*
				 * Waiting for result of "SELECT pg_is_in_recovery()".  We
				 * must transiently set status = CONNECTION_OK in order to use
				 * the result-consuming subroutines.
				 */
				conn->status = CONNECTION_OK;
				if (!PQconsumeInput(conn))
					goto error_return;

				if (PQisBusy(conn))
				{
					conn->status = CONNECTION_CHECK_STANDBY;
					return PGRES_POLLING_READING;
				}

				res = PQgetResult(conn);
				if (res && PQresultStatus(res) == PGRES_TUPLES_OK &&
					PQntuples(res) == 1)
				{
					char	   *val = PQgetvalue(res, 0, 0);

					if (strncmp(val, "t", 1) == 0)
						conn->in_hot_standby = PG_BOOL_YES;
					else
						conn->in_hot_standby = PG_BOOL_NO;
					PQclear(res);

					/* Finish reading messages before continuing */
					conn->status = CONNECTION_CONSUME;
					goto keep_going;
				}

				/* Something went wrong with "SELECT pg_is_in_recovery()". */
				if (res)
					PQclear(res);

				/* Append error report to conn->errorMessage. */
				appendPQExpBuffer(&conn->errorMessage,
								  libpq_gettext("\"%s\" failed\n"),
								  "SELECT pg_is_in_recovery()");

				/* Close connection politely. */
				conn->status = CONNECTION_OK;
				sendTerminateConn(conn);

				/* Try next host. */
				conn->try_next_host = true;
				goto keep_going;
			}

		default:
			appendPQExpBuffer(&conn->errorMessage,
							  libpq_gettext("invalid connection state %d, "
											"probably indicative of memory corruption\n"),
							  conn->status);
			goto error_return;
	}

	/* Unreachable */

error_return:

	/*
	 * We used to close the socket at this point, but that makes it awkward
	 * for those above us if they wish to remove this socket from their own
	 * records (an fd_set for example).  We'll just have this socket closed
	 * when PQfinish is called (which is compulsory even after an error, since
	 * the connection structure must be freed).
	 */
	conn->status = CONNECTION_BAD;
	return PGRES_POLLING_FAILED;
}