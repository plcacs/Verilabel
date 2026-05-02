static int client_func(const test_param_t *test)
{
	struct gg_session *gs;
	struct gg_login_params glp;
	char tmp;

	gg_proxy_host = HOST_PROXY;
	gg_proxy_port = 8080;
	gg_proxy_enabled = test->proxy_mode;

	memset(&glp, 0, sizeof(glp));
	glp.uin = 1;
	glp.password = "dupa.8";
	glp.async = test->async_mode;
	glp.resolver = GG_RESOLVER_PTHREAD;

	if (test->server)
		glp.server_addr = inet_addr(HOST_LOCAL);

	if (test->ssl_mode)
		glp.tls = GG_SSL_REQUIRED;

	while (read(timeout_pipe[0], &tmp, 1) != -1);

	gs = gg_login(&glp);

	if (gs == NULL)
		return 0;

	if (!test->async_mode) {
		gg_free_session(gs);
		return 1;
	} else {
		for (;;) {
			fd_set rd, wr;
			int res;
			int max_fd;
			struct timeval *tv_ptr = NULL;

#ifdef CLIENT_TIMEOUT
			struct timeval tv;

			tv.tv_sec = CLIENT_TIMEOUT;
			tv.tv_usec = 0;
			tv_ptr = &tv;
#endif

			FD_ZERO(&rd);
			FD_ZERO(&wr);

			max_fd = timeout_pipe[0];

			if (gs->fd > max_fd)
				max_fd = gs->fd;

			FD_SET(timeout_pipe[0], &rd);

			if ((gs->check & GG_CHECK_READ))
				FD_SET(gs->fd, &rd);

			if ((gs->check & GG_CHECK_WRITE))
				FD_SET(gs->fd, &wr);

			res = select(max_fd + 1, &rd, &wr, NULL, tv_ptr);

			if (res == 0) {
				debug("Test timeout\n");
				gg_free_session(gs);
				return 0;
			}
			
			if (res == -1 && errno != EINTR) {
				debug("select() failed: %s\n", strerror(errno));
				gg_free_session(gs);
				return -1;
			}
			if (res == -1)
				continue;

			if (FD_ISSET(timeout_pipe[0], &rd)) {
				if (read(timeout_pipe[0], &tmp, 1) != 1) {
					debug("Test error\n");
					gg_free_session(gs);
					return -1;
				}

				if (!gs->soft_timeout) {
					debug("Hard timeout\n");
					gg_free_session(gs);
					return 0;
				}
			}

			if (FD_ISSET(gs->fd, &rd) || FD_ISSET(gs->fd, &wr) || (FD_ISSET(timeout_pipe[0], &rd) && gs->soft_timeout)) {
				struct gg_event *ge;
				
				if (FD_ISSET(timeout_pipe[0], &rd)) {
					debug("Soft timeout\n");
					gs->timeout = 0;
				}
		
				ge = gg_watch_fd(gs);

				if (!ge) {
					debug("gg_watch_fd() failed\n");
					gg_free_session(gs);
					return -1;
				}

				switch (ge->type) {
					case GG_EVENT_CONN_SUCCESS:
						gg_event_free(ge);
						gg_free_session(gs);
						return 1;

					case GG_EVENT_CONN_FAILED:
						gg_event_free(ge);
						gg_free_session(gs);
						return 0;

					case GG_EVENT_NONE:
						break;

					default:
						debug("Unknown event %d\n", ge->type);
						gg_event_free(ge);
						gg_free_session(gs);
						return -1;
				}

				gg_event_free(ge);
			}
		}
	}
}