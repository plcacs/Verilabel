static void wsgi_setup_access(WSGIDaemonProcess *daemon)
{
    /* Setup the umask for the effective user. */

    if (daemon->group->umask != -1)
        umask(daemon->group->umask);

    /* Change to chroot environment. */

    if (daemon->group->root) {
        if (chroot(daemon->group->root) == -1) {
            ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                         "mod_wsgi (pid=%d): Unable to change root "
                         "directory to '%s'.", getpid(), daemon->group->root);
        }
    }

    /* Setup the working directory.*/

    if (daemon->group->home) {
        if (chdir(daemon->group->home) == -1) {
            ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                         "mod_wsgi (pid=%d): Unable to change working "
                         "directory to '%s'.", getpid(), daemon->group->home);
        }
    }
    else if (geteuid()) {
        struct passwd *pwent;

        pwent = getpwuid(geteuid());

        if (pwent) {
            if (chdir(pwent->pw_dir) == -1) {
                ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                             "mod_wsgi (pid=%d): Unable to change working "
                             "directory to '%s'.", getpid(), pwent->pw_dir);
            }
        }
        else {
            ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                         "mod_wsgi (pid=%d): Unable to determine home "
                         "directory for uid=%ld.", getpid(), (long)geteuid());
        }
    }
    else {
        struct passwd *pwent;

        pwent = getpwuid(daemon->group->uid);

        if (pwent) {
            if (chdir(pwent->pw_dir) == -1) {
                ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                             "mod_wsgi (pid=%d): Unable to change working "
                             "directory to '%s'.", getpid(), pwent->pw_dir);
            }
        }
        else {
            ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                         "mod_wsgi (pid=%d): Unable to determine home "
                         "directory for uid=%ld.", getpid(),
                         (long)daemon->group->uid);
        }
    }

    /* Don't bother switch user/group if not root. */

    if (geteuid())
        return;

    /* Setup the daemon process real and effective group. */

    if (setgid(daemon->group->gid) == -1) {
        ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                     "mod_wsgi (pid=%d): Unable to set group id to gid=%u.",
                     getpid(), (unsigned)daemon->group->gid);
    }
    else {
        if (daemon->group->groups) {
            if (setgroups(daemon->group->groups_count,
                          daemon->group->groups) == -1) {
                ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno),
                             wsgi_server, "mod_wsgi (pid=%d): Unable "
                             "to set supplementary groups for uname=%s "
                             "of '%s'.", getpid(), daemon->group->user,
                             daemon->group->groups_list);
            }
        }
        else if (initgroups(daemon->group->user, daemon->group->gid) == -1) {
            ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno),
                         wsgi_server, "mod_wsgi (pid=%d): Unable "
                         "to set groups for uname=%s and gid=%u.", getpid(),
                         daemon->group->user, (unsigned)daemon->group->gid);
        }
    }

    /* Setup the daemon process real and effective user. */

    if (setuid(daemon->group->uid) == -1) {
        ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                     "mod_wsgi (pid=%d): Unable to change to uid=%ld.",
                     getpid(), (long)daemon->group->uid);
    }

    /*
     * Linux prevents generation of core dumps after setuid()
     * has been used. Attempt to reenable ability to dump core
     * so that the CoreDumpDirectory directive still works.
     */

#if defined(HAVE_PRCTL) && defined(PR_SET_DUMPABLE)
    /* This applies to Linux 2.4 and later. */
    if (ap_coredumpdir_configured) {
        if (prctl(PR_SET_DUMPABLE, 1)) {
            ap_log_error(APLOG_MARK, WSGI_LOG_ALERT(errno), wsgi_server,
                    "mod_wsgi (pid=%d): Set dumpable failed. This child "
                    "will not coredump after software errors.", getpid());
        }
    }
#endif
}