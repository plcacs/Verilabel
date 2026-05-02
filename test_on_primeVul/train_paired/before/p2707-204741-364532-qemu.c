int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se;
    struct fuse_cmdline_opts opts;
    struct lo_data lo = {
        .sandbox = SANDBOX_NAMESPACE,
        .debug = 0,
        .writeback = 0,
        .posix_lock = 0,
        .allow_direct_io = 0,
        .proc_self_fd = -1,
        .user_killpriv_v2 = -1,
        .user_posix_acl = -1,
    };
    struct lo_map_elem *root_elem;
    struct lo_map_elem *reserve_elem;
    int ret = -1;

    /* Initialize time conversion information for localtime_r(). */
    tzset();

    /* Don't mask creation mode, kernel already did that */
    umask(0);

    qemu_init_exec_dir(argv[0]);

    pthread_mutex_init(&lo.mutex, NULL);
    lo.inodes = g_hash_table_new(lo_key_hash, lo_key_equal);
    lo.root.fd = -1;
    lo.root.fuse_ino = FUSE_ROOT_ID;
    lo.cache = CACHE_AUTO;

    /*
     * Set up the ino map like this:
     * [0] Reserved (will not be used)
     * [1] Root inode
     */
    lo_map_init(&lo.ino_map);
    reserve_elem = lo_map_reserve(&lo.ino_map, 0);
    if (!reserve_elem) {
        fuse_log(FUSE_LOG_ERR, "failed to alloc reserve_elem.\n");
        goto err_out1;
    }
    reserve_elem->in_use = false;
    root_elem = lo_map_reserve(&lo.ino_map, lo.root.fuse_ino);
    if (!root_elem) {
        fuse_log(FUSE_LOG_ERR, "failed to alloc root_elem.\n");
        goto err_out1;
    }
    root_elem->inode = &lo.root;

    lo_map_init(&lo.dirp_map);
    lo_map_init(&lo.fd_map);

    if (fuse_parse_cmdline(&args, &opts) != 0) {
        goto err_out1;
    }
    fuse_set_log_func(log_func);
    use_syslog = opts.syslog;
    if (use_syslog) {
        openlog("virtiofsd", LOG_PID, LOG_DAEMON);
    }

    if (opts.show_help) {
        printf("usage: %s [options]\n\n", argv[0]);
        fuse_cmdline_help();
        printf("    -o source=PATH             shared directory tree\n");
        fuse_lowlevel_help();
        ret = 0;
        goto err_out1;
    } else if (opts.show_version) {
        qemu_version();
        fuse_lowlevel_version();
        ret = 0;
        goto err_out1;
    } else if (opts.print_capabilities) {
        print_capabilities();
        ret = 0;
        goto err_out1;
    }

    if (fuse_opt_parse(&args, &lo, lo_opts, NULL) == -1) {
        goto err_out1;
    }

    if (opts.log_level != 0) {
        current_log_level = opts.log_level;
    } else {
        /* default log level is INFO */
        current_log_level = FUSE_LOG_INFO;
    }
    lo.debug = opts.debug;
    if (lo.debug) {
        current_log_level = FUSE_LOG_DEBUG;
    }
    if (lo.source) {
        struct stat stat;
        int res;

        res = lstat(lo.source, &stat);
        if (res == -1) {
            fuse_log(FUSE_LOG_ERR, "failed to stat source (\"%s\"): %m\n",
                     lo.source);
            exit(1);
        }
        if (!S_ISDIR(stat.st_mode)) {
            fuse_log(FUSE_LOG_ERR, "source is not a directory\n");
            exit(1);
        }
    } else {
        lo.source = strdup("/");
        if (!lo.source) {
            fuse_log(FUSE_LOG_ERR, "failed to strdup source\n");
            goto err_out1;
        }
    }

    if (lo.xattrmap) {
        lo.xattr = 1;
        parse_xattrmap(&lo);
    }

    if (!lo.timeout_set) {
        switch (lo.cache) {
        case CACHE_NONE:
            lo.timeout = 0.0;
            break;

        case CACHE_AUTO:
            lo.timeout = 1.0;
            break;

        case CACHE_ALWAYS:
            lo.timeout = 86400.0;
            break;
        }
    } else if (lo.timeout < 0) {
        fuse_log(FUSE_LOG_ERR, "timeout is negative (%lf)\n", lo.timeout);
        exit(1);
    }

    if (lo.user_posix_acl == 1 && !lo.xattr) {
        fuse_log(FUSE_LOG_ERR, "Can't enable posix ACLs. xattrs are disabled."
                 "\n");
        exit(1);
    }

    lo.use_statx = true;

    se = fuse_session_new(&args, &lo_oper, sizeof(lo_oper), &lo);
    if (se == NULL) {
        goto err_out1;
    }

    if (fuse_set_signal_handlers(se) != 0) {
        goto err_out2;
    }

    if (fuse_session_mount(se) != 0) {
        goto err_out3;
    }

    fuse_daemonize(opts.foreground);

    setup_nofile_rlimit(opts.rlimit_nofile);

    /* Must be before sandbox since it wants /proc */
    setup_capng();

    setup_sandbox(&lo, se, opts.syslog);

    setup_root(&lo, &lo.root);
    /* Block until ctrl+c or fusermount -u */
    ret = virtio_loop(se);

    fuse_session_unmount(se);
    cleanup_capng();
err_out3:
    fuse_remove_signal_handlers(se);
err_out2:
    fuse_session_destroy(se);
err_out1:
    fuse_opt_free_args(&args);

    fuse_lo_data_cleanup(&lo);

    return ret ? 1 : 0;
}