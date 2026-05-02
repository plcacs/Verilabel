static void setup_namespaces(struct lo_data *lo, struct fuse_session *se)
{
    pid_t child;

    /*
     * Create a new pid namespace for *child* processes.  We'll have to
     * fork in order to enter the new pid namespace.  A new mount namespace
     * is also needed so that we can remount /proc for the new pid
     * namespace.
     *
     * Our UNIX domain sockets have been created.  Now we can move to
     * an empty network namespace to prevent TCP/IP and other network
     * activity in case this process is compromised.
     */
    if (unshare(CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET) != 0) {
        fuse_log(FUSE_LOG_ERR, "unshare(CLONE_NEWPID | CLONE_NEWNS): %m\n");
        exit(1);
    }

    child = fork();
    if (child < 0) {
        fuse_log(FUSE_LOG_ERR, "fork() failed: %m\n");
        exit(1);
    }
    if (child > 0) {
        pid_t waited;
        int wstatus;

        setup_wait_parent_capabilities();

        /* The parent waits for the child */
        do {
            waited = waitpid(child, &wstatus, 0);
        } while (waited < 0 && errno == EINTR && !se->exited);

        /* We were terminated by a signal, see fuse_signals.c */
        if (se->exited) {
            exit(0);
        }

        if (WIFEXITED(wstatus)) {
            exit(WEXITSTATUS(wstatus));
        }

        exit(1);
    }

    /* Send us SIGTERM when the parent thread terminates, see prctl(2) */
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    /*
     * If the mounts have shared propagation then we want to opt out so our
     * mount changes don't affect the parent mount namespace.
     */
    if (mount(NULL, "/", NULL, MS_REC | MS_SLAVE, NULL) < 0) {
        fuse_log(FUSE_LOG_ERR, "mount(/, MS_REC|MS_SLAVE): %m\n");
        exit(1);
    }

    /* The child must remount /proc to use the new pid namespace */
    if (mount("proc", "/proc", "proc",
              MS_NODEV | MS_NOEXEC | MS_NOSUID | MS_RELATIME, NULL) < 0) {
        fuse_log(FUSE_LOG_ERR, "mount(/proc): %m\n");
        exit(1);
    }

    /*
     * We only need /proc/self/fd. Prevent ".." from accessing parent
     * directories of /proc/self/fd by bind-mounting it over /proc. Since / was
     * previously remounted with MS_REC | MS_SLAVE this mount change only
     * affects our process.
     */
    if (mount("/proc/self/fd", "/proc", NULL, MS_BIND, NULL) < 0) {
        fuse_log(FUSE_LOG_ERR, "mount(/proc/self/fd, MS_BIND): %m\n");
        exit(1);
    }

    /* Get the /proc (actually /proc/self/fd, see above) file descriptor */
    lo->proc_self_fd = open("/proc", O_PATH);
    if (lo->proc_self_fd == -1) {
        fuse_log(FUSE_LOG_ERR, "open(/proc, O_PATH): %m\n");
        exit(1);
    }
}