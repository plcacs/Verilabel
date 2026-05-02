static void setup_namespaces(struct lo_data *lo, struct fuse_session *se)
{
    pid_t child;
    char template[] = "virtiofsd-XXXXXX";
    char *tmpdir;

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

    tmpdir = mkdtemp(template);
    if (!tmpdir) {
        fuse_log(FUSE_LOG_ERR, "tmpdir(%s): %m\n", template);
        exit(1);
    }

    if (mount("/proc/self/fd", tmpdir, NULL, MS_BIND, NULL) < 0) {
        fuse_log(FUSE_LOG_ERR, "mount(/proc/self/fd, %s, MS_BIND): %m\n",
                 tmpdir);
        exit(1);
    }

    /* Now we can get our /proc/self/fd directory file descriptor */
    lo->proc_self_fd = open(tmpdir, O_PATH);
    if (lo->proc_self_fd == -1) {
        fuse_log(FUSE_LOG_ERR, "open(%s, O_PATH): %m\n", tmpdir);
        exit(1);
    }

    if (umount2(tmpdir, MNT_DETACH) < 0) {
        fuse_log(FUSE_LOG_ERR, "umount2(%s, MNT_DETACH): %m\n", tmpdir);
        exit(1);
    }

    if (rmdir(tmpdir) < 0) {
        fuse_log(FUSE_LOG_ERR, "rmdir(%s): %m\n", tmpdir);
    }
}