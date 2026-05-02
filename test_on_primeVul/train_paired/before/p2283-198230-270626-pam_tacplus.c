int _pam_parse(int argc, const char **argv) {
    int ctrl = 0;
    const char *current_secret = NULL;

    /* otherwise the list will grow with each call */
    memset(tac_srv, 0, sizeof(tacplus_server_t) * TAC_PLUS_MAXSERVERS);
    memset(&tac_srv_addr, 0, sizeof(struct addrinfo) * TAC_PLUS_MAXSERVERS);
    memset(&tac_sock_addr, 0, sizeof(struct sockaddr) * TAC_PLUS_MAXSERVERS);
    memset(&tac_sock6_addr, 0, sizeof(struct sockaddr_in6) * TAC_PLUS_MAXSERVERS);
    tac_srv_no = 0;

    tac_service[0] = 0;
    tac_protocol[0] = 0;
    tac_prompt[0] = 0;
    tac_login[0] = 0;

    for (ctrl = 0; argc-- > 0; ++argv) {
        if (!strcmp(*argv, "debug")) { /* all */
            ctrl |= PAM_TAC_DEBUG;
        } else if (!strcmp(*argv, "use_first_pass")) {
            ctrl |= PAM_TAC_USE_FIRST_PASS;
        } else if (!strcmp(*argv, "try_first_pass")) {
            ctrl |= PAM_TAC_TRY_FIRST_PASS;
        } else if (!strncmp(*argv, "service=", 8)) { /* author & acct */
            xstrcpy(tac_service, *argv + 8, sizeof(tac_service));
        } else if (!strncmp(*argv, "protocol=", 9)) { /* author & acct */
            xstrcpy(tac_protocol, *argv + 9, sizeof(tac_protocol));
        } else if (!strncmp(*argv, "prompt=", 7)) { /* authentication */
            xstrcpy(tac_prompt, *argv + 7, sizeof(tac_prompt));
            /* Replace _ with space */
            unsigned long chr;
            for (chr = 0; chr < strlen(tac_prompt); chr++) {
                if (tac_prompt[chr] == '_') {
                    tac_prompt[chr] = ' ';
                }
            }
        } else if (!strncmp(*argv, "login=", 6)) {
            xstrcpy(tac_login, *argv + 6, sizeof(tac_login));
        } else if (!strcmp(*argv, "acct_all")) {
            ctrl |= PAM_TAC_ACCT;
        } else if (!strncmp(*argv, "server=", 7)) { /* authen & acct */
            if (tac_srv_no < TAC_PLUS_MAXSERVERS) {
                struct addrinfo hints, *servers, *server;
                int rv;
                char *close_bracket, *server_name, *port, server_buf[256];

                memset(&hints, 0, sizeof hints);
                memset(&server_buf, 0, sizeof(server_buf));
                hints.ai_family = AF_UNSPEC;  /* use IPv4 or IPv6, whichever */
                hints.ai_socktype = SOCK_STREAM;

                if (strlen(*argv + 7) >= sizeof(server_buf)) {
                    _pam_log(LOG_ERR, "server address too long, sorry");
                    continue;
                }
                strcpy(server_buf, *argv + 7);

                if (*server_buf == '[' &&
                    (close_bracket = strchr(server_buf, ']')) != NULL) { /* Check for URI syntax */
                    server_name = server_buf + 1;
                    _pam_log (LOG_ERR,
                        "reading server address as: %s ",
                        server_name);
                    port = strchr(close_bracket, ':');
                    *close_bracket = '\0';
                } else { /* Fall back to traditional syntax */
                    server_name = server_buf;
                    port = strchr(server_buf, ':');
                }
                if (port != NULL) {
                    *port = '\0';
                    port++;
                }
                _pam_log (LOG_DEBUG,
                        "sending server address to getaddrinfo as: %s ",
                        server_name);
                if ((rv = getaddrinfo(server_name, (port == NULL) ? "49" : port, &hints, &servers)) == 0) {
                    for (server = servers;
                         server != NULL && tac_srv_no < TAC_PLUS_MAXSERVERS; server = server->ai_next) {
                        set_tac_srv_addr(tac_srv_no, server);
                        set_tac_srv_key(tac_srv_no, current_secret);
                        tac_srv_no++;
                    }
                    _pam_log(LOG_DEBUG, "%s: server index %d ", __FUNCTION__, tac_srv_no);
                    freeaddrinfo (servers);
                } else {
                    _pam_log(LOG_ERR,
                             "skip invalid server: %s (getaddrinfo: %s)",
                             server_name, gai_strerror(rv));
                }
            } else {
                _pam_log(LOG_ERR, "maximum number of servers (%d) exceeded, skipping",
                         TAC_PLUS_MAXSERVERS);
            }
        } else if (!strncmp(*argv, "secret=", 7)) {
            current_secret = *argv + 7;     /* points right into argv (which is const) */

            // this is possible because server structure is initialized only on the server= occurence
            if (tac_srv_no == 0) {
                _pam_log(LOG_ERR, "secret set but no servers configured yet");
            } else {
                // set secret for the last server configured
                set_tac_srv_key(tac_srv_no - 1, current_secret);
            }
        } else if (!strncmp(*argv, "timeout=", 8)) {

#ifdef HAVE_STRTOL
            tac_timeout = strtol(*argv + 8, NULL, 10);

#else
            tac_timeout = atoi(*argv + 8);
#endif
            if (tac_timeout == LONG_MAX) {
                _pam_log(LOG_ERR, "timeout parameter cannot be parsed as integer: %s", *argv);
                tac_timeout = 0;
            } else {
                tac_readtimeout_enable = 1;
            }
        } else {
            _pam_log(LOG_WARNING, "unrecognized option: %s", *argv);
        }
    }

    if (ctrl & PAM_TAC_DEBUG) {
        unsigned long n;

        _pam_log(LOG_DEBUG, "%d servers defined", tac_srv_no);

        for (n = 0; n < tac_srv_no; n++) {
            _pam_log(LOG_DEBUG, "server[%lu] { addr=%s, key='%s' }", n, tac_ntop(tac_srv[n].addr->ai_addr),
                     tac_srv[n].key);
        }

        _pam_log(LOG_DEBUG, "tac_service='%s'", tac_service);
        _pam_log(LOG_DEBUG, "tac_protocol='%s'", tac_protocol);
        _pam_log(LOG_DEBUG, "tac_prompt='%s'", tac_prompt);
        _pam_log(LOG_DEBUG, "tac_login='%s'", tac_login);
    }

    return ctrl;
}    /* _pam_parse */