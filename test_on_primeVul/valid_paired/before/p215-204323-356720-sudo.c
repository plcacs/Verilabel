sudoers_policy_deserialize_info(void *v)
{
    struct sudoers_open_info *info = v;
    char * const *cur;
    const char *p, *errstr, *groups = NULL;
    const char *remhost = NULL;
    int flags = 0;
    debug_decl(sudoers_policy_deserialize_info, SUDOERS_DEBUG_PLUGIN);

#define MATCHES(s, v)	\
    (strncmp((s), (v), sizeof(v) - 1) == 0)

#define INVALID(v) do {	\
    sudo_warn(U_("invalid %.*s set by sudo front-end"), \
	(int)(sizeof(v) - 2), (v)); \
} while (0)

#define CHECK(s, v) do {	\
    if ((s)[sizeof(v) - 1] == '\0') { \
	INVALID(v); \
	goto bad; \
    } \
} while (0)

    if (sudo_gettime_real(&sudo_user.submit_time) == -1) {
	sudo_warn("%s", U_("unable to get time of day"));
	goto bad;
    }

    /* Parse sudo.conf plugin args. */
    if (info->plugin_args != NULL) {
	for (cur = info->plugin_args; *cur != NULL; cur++) {
	    if (MATCHES(*cur, "error_recovery=")) {
		int val = sudo_strtobool(*cur + sizeof("error_recovery=") - 1);
		if (val == -1) {
		    INVALID("error_recovery=");	/* Not a fatal error. */
		} else {
		    sudoers_recovery = val;
		}
		continue;
	    }
	    if (MATCHES(*cur, "sudoers_file=")) {
		CHECK(*cur, "sudoers_file=");
		sudoers_file = *cur + sizeof("sudoers_file=") - 1;
		continue;
	    }
	    if (MATCHES(*cur, "sudoers_uid=")) {
		p = *cur + sizeof("sudoers_uid=") - 1;
		sudoers_uid = (uid_t) sudo_strtoid(p, &errstr);
		if (errstr != NULL) {
		    sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		    goto bad;
		}
		continue;
	    }
	    if (MATCHES(*cur, "sudoers_gid=")) {
		p = *cur + sizeof("sudoers_gid=") - 1;
		sudoers_gid = (gid_t) sudo_strtoid(p, &errstr);
		if (errstr != NULL) {
		    sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		    goto bad;
		}
		continue;
	    }
	    if (MATCHES(*cur, "sudoers_mode=")) {
		p = *cur + sizeof("sudoers_mode=") - 1;
		sudoers_mode = sudo_strtomode(p, &errstr);
		if (errstr != NULL) {
		    sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		    goto bad;
		}
		continue;
	    }
	    if (MATCHES(*cur, "ldap_conf=")) {
		CHECK(*cur, "ldap_conf=");
		path_ldap_conf = *cur + sizeof("ldap_conf=") - 1;
		continue;
	    }
	    if (MATCHES(*cur, "ldap_secret=")) {
		CHECK(*cur, "ldap_secret=");
		path_ldap_secret = *cur + sizeof("ldap_secret=") - 1;
		continue;
	    }
	}
    }

    /* Parse command line settings. */
    user_closefrom = -1;
    for (cur = info->settings; *cur != NULL; cur++) {
	if (MATCHES(*cur, "closefrom=")) {
	    errno = 0;
	    p = *cur + sizeof("closefrom=") - 1;
	    user_closefrom = sudo_strtonum(p, 3, INT_MAX, &errstr);
	    if (user_closefrom == 0) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "cmnd_chroot=")) {
	    CHECK(*cur, "cmnd_chroot=");
	    user_runchroot = *cur + sizeof("cmnd_chroot=") - 1;
	    continue;
	}
	if (MATCHES(*cur, "cmnd_cwd=")) {
	    CHECK(*cur, "cmnd_cwd=");
	    user_runcwd = *cur + sizeof("cmnd_cwd=") - 1;
	    continue;
	}
	if (MATCHES(*cur, "runas_user=")) {
	    CHECK(*cur, "runas_user=");
	    sudo_user.runas_user = *cur + sizeof("runas_user=") - 1;
	    SET(sudo_user.flags, RUNAS_USER_SPECIFIED);
	    continue;
	}
	if (MATCHES(*cur, "runas_group=")) {
	    CHECK(*cur, "runas_group=");
	    sudo_user.runas_group = *cur + sizeof("runas_group=") - 1;
	    SET(sudo_user.flags, RUNAS_GROUP_SPECIFIED);
	    continue;
	}
	if (MATCHES(*cur, "prompt=")) {
	    /* Allow epmpty prompt. */
	    user_prompt = *cur + sizeof("prompt=") - 1;
	    def_passprompt_override = true;
	    continue;
	}
	if (MATCHES(*cur, "set_home=")) {
	    if (parse_bool(*cur, sizeof("set_home") - 1, &flags,
		MODE_RESET_HOME) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "preserve_environment=")) {
	    if (parse_bool(*cur, sizeof("preserve_environment") - 1, &flags,
		MODE_PRESERVE_ENV) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "run_shell=")) {
	    if (parse_bool(*cur, sizeof("run_shell") -1, &flags,
		MODE_SHELL) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "login_shell=")) {
	    if (parse_bool(*cur, sizeof("login_shell") - 1, &flags,
		MODE_LOGIN_SHELL) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "implied_shell=")) {
	    if (parse_bool(*cur, sizeof("implied_shell") - 1, &flags,
		MODE_IMPLIED_SHELL) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "preserve_groups=")) {
	    if (parse_bool(*cur, sizeof("preserve_groups") - 1, &flags,
		MODE_PRESERVE_GROUPS) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "ignore_ticket=")) {
	    if (parse_bool(*cur, sizeof("ignore_ticket") -1, &flags,
		MODE_IGNORE_TICKET) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "noninteractive=")) {
	    if (parse_bool(*cur, sizeof("noninteractive") - 1, &flags,
		MODE_NONINTERACTIVE) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "sudoedit=")) {
	    if (parse_bool(*cur, sizeof("sudoedit") - 1, &flags,
		MODE_EDIT) == -1)
		goto bad;
	    continue;
	}
	if (MATCHES(*cur, "login_class=")) {
	    CHECK(*cur, "login_class=");
	    login_class = *cur + sizeof("login_class=") - 1;
	    def_use_loginclass = true;
	    continue;
	}
#ifdef HAVE_PRIV_SET
	if (MATCHES(*cur, "runas_privs=")) {
	    CHECK(*cur, "runas_privs=");
	    def_privs = *cur + sizeof("runas_privs=") - 1;
	    continue;
	}
	if (MATCHES(*cur, "runas_limitprivs=")) {
	    CHECK(*cur, "runas_limitprivs=");
	    def_limitprivs = *cur + sizeof("runas_limitprivs=") - 1;
	    continue;
	}
#endif /* HAVE_PRIV_SET */
#ifdef HAVE_SELINUX
	if (MATCHES(*cur, "selinux_role=")) {
	    CHECK(*cur, "selinux_role=");
	    user_role = *cur + sizeof("selinux_role=") - 1;
	    continue;
	}
	if (MATCHES(*cur, "selinux_type=")) {
	    CHECK(*cur, "selinux_type=");
	    user_type = *cur + sizeof("selinux_type=") - 1;
	    continue;
	}
#endif /* HAVE_SELINUX */
#ifdef HAVE_BSD_AUTH_H
	if (MATCHES(*cur, "bsdauth_type=")) {
	    CHECK(*cur, "login_style=");
	    login_style = *cur + sizeof("bsdauth_type=") - 1;
	    continue;
	}
#endif /* HAVE_BSD_AUTH_H */
	if (MATCHES(*cur, "network_addrs=")) {
	    interfaces_string = *cur + sizeof("network_addrs=") - 1;
	    if (!set_interfaces(interfaces_string)) {
		sudo_warn("%s", U_("unable to parse network address list"));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "max_groups=")) {
	    errno = 0;
	    p = *cur + sizeof("max_groups=") - 1;
	    sudo_user.max_groups = sudo_strtonum(p, 1, INT_MAX, &errstr);
	    if (sudo_user.max_groups == 0) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "remote_host=")) {
	    CHECK(*cur, "remote_host=");
	    remhost = *cur + sizeof("remote_host=") - 1;
	    continue;
	}
	if (MATCHES(*cur, "timeout=")) {
	    p = *cur + sizeof("timeout=") - 1;
	    user_timeout = parse_timeout(p);
	    if (user_timeout == -1) {
		if (errno == ERANGE)
		    sudo_warnx(U_("%s: %s"), p, U_("timeout value too large"));
		else
		    sudo_warnx(U_("%s: %s"), p, U_("invalid timeout value"));
		goto bad;
	    }
	    continue;
	}
#ifdef ENABLE_SUDO_PLUGIN_API
	if (MATCHES(*cur, "plugin_dir=")) {
	    CHECK(*cur, "plugin_dir=");
	    path_plugin_dir = *cur + sizeof("plugin_dir=") - 1;
	    continue;
	}
#endif
    }

    user_gid = (gid_t)-1;
    user_sid = (pid_t)-1;
    user_uid = (gid_t)-1;
    user_umask = (mode_t)-1;
    for (cur = info->user_info; *cur != NULL; cur++) {
	if (MATCHES(*cur, "user=")) {
	    CHECK(*cur, "user=");
	    if ((user_name = strdup(*cur + sizeof("user=") - 1)) == NULL)
		goto oom;
	    continue;
	}
	if (MATCHES(*cur, "uid=")) {
	    p = *cur + sizeof("uid=") - 1;
	    user_uid = (uid_t) sudo_strtoid(p, &errstr);
	    if (errstr != NULL) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "gid=")) {
	    p = *cur + sizeof("gid=") - 1;
	    user_gid = (gid_t) sudo_strtoid(p, &errstr);
	    if (errstr != NULL) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "groups=")) {
	    CHECK(*cur, "groups=");
	    groups = *cur + sizeof("groups=") - 1;
	    continue;
	}
	if (MATCHES(*cur, "cwd=")) {
	    CHECK(*cur, "cwd=");
	    if ((user_cwd = strdup(*cur + sizeof("cwd=") - 1)) == NULL)
		goto oom;
	    continue;
	}
	if (MATCHES(*cur, "tty=")) {
	    CHECK(*cur, "tty=");
	    if ((user_ttypath = strdup(*cur + sizeof("tty=") - 1)) == NULL)
		goto oom;
	    user_tty = user_ttypath;
	    if (strncmp(user_tty, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
		user_tty += sizeof(_PATH_DEV) - 1;
	    continue;
	}
	if (MATCHES(*cur, "host=")) {
	    CHECK(*cur, "host=");
	    if ((user_host = strdup(*cur + sizeof("host=") - 1)) == NULL)
		goto oom;
	    if ((p = strchr(user_host, '.')) != NULL) {
		user_shost = strndup(user_host, (size_t)(p - user_host));
		if (user_shost == NULL)
		    goto oom;
	    } else {
		user_shost = user_host;
	    }
	    continue;
	}
	if (MATCHES(*cur, "lines=")) {
	    errno = 0;
	    p = *cur + sizeof("lines=") - 1;
	    sudo_user.lines = sudo_strtonum(p, 1, INT_MAX, &errstr);
	    if (sudo_user.lines == 0) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "cols=")) {
	    errno = 0;
	    p = *cur + sizeof("cols=") - 1;
	    sudo_user.cols = sudo_strtonum(p, 1, INT_MAX, &errstr);
	    if (sudo_user.cols == 0) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "sid=")) {
	    p = *cur + sizeof("sid=") - 1;
	    user_sid = (pid_t) sudo_strtoid(p, &errstr);
	    if (errstr != NULL) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
	if (MATCHES(*cur, "umask=")) {
	    p = *cur + sizeof("umask=") - 1;
	    sudo_user.umask = sudo_strtomode(p, &errstr);
	    if (errstr != NULL) {
		sudo_warnx(U_("%s: %s"), *cur, U_(errstr));
		goto bad;
	    }
	    continue;
	}
    }

    /* User name, user-ID, group-ID and host name must be specified. */
    if (user_name == NULL) {
	sudo_warnx("%s", U_("user name not set by sudo front-end"));
	goto bad;
    }
    if (user_uid == (uid_t)-1) {
	sudo_warnx("%s", U_("user-ID not set by sudo front-end"));
	goto bad;
    }
    if (user_gid == (gid_t)-1) {
	sudo_warnx("%s", U_("group-ID not set by sudo front-end"));
	goto bad;
    }
    if (user_host == NULL) {
	sudo_warnx("%s", U_("host name not set by sudo front-end"));
	goto bad;
    }

    if ((user_runhost = strdup(remhost ? remhost : user_host)) == NULL)
	goto oom;
    if ((p = strchr(user_runhost, '.')) != NULL) {
	user_srunhost = strndup(user_runhost, (size_t)(p - user_runhost));
	if (user_srunhost == NULL)
	    goto oom;
    } else {
	user_srunhost = user_runhost;
    }
    if (user_cwd == NULL) {
	if ((user_cwd = strdup("unknown")) == NULL)
	    goto oom;
    }
    if (user_runcwd == NULL) {
	if ((user_runcwd = strdup(user_cwd)) == NULL)
	    goto oom;
    }
    if (user_tty == NULL) {
	if ((user_tty = strdup("unknown")) == NULL)
	    goto oom;
	/* user_ttypath remains NULL */
    }

    if (groups != NULL) {
	/* sudo_parse_gids() will print a warning on error. */
	user_ngids = sudo_parse_gids(groups, &user_gid, &user_gids);
	if (user_ngids == -1)
	    goto bad;
    }

    /* umask is only set in user_info[] for API 1.10 and above. */
    if (user_umask == (mode_t)-1) {
	user_umask = umask(0);
	umask(user_umask);
    }

    /* Always reset the environment for a login shell. */
    if (ISSET(flags, MODE_LOGIN_SHELL))
	def_env_reset = true;

    /* Some systems support fexecve() which we use for digest matches. */
    cmnd_fd = -1;

    /* Dump settings and user info (XXX - plugin args) */
    for (cur = info->settings; *cur != NULL; cur++)
	sudo_debug_printf(SUDO_DEBUG_INFO, "settings: %s", *cur);
    for (cur = info->user_info; *cur != NULL; cur++)
	sudo_debug_printf(SUDO_DEBUG_INFO, "user_info: %s", *cur);

#undef MATCHES
#undef INVALID
#undef CHECK
    debug_return_int(flags);

oom:
    sudo_warnx(U_("%s: %s"), __func__, U_("unable to allocate memory"));
bad:
    debug_return_int(MODE_ERROR);
}