int button_open(Button *b) {
        char *p, name[256];
        int r;

        assert(b);

        b->fd = safe_close(b->fd);

        p = strjoina("/dev/input/", b->name);

        b->fd = open(p, O_RDWR|O_CLOEXEC|O_NOCTTY|O_NONBLOCK);
        if (b->fd < 0)
                return log_warning_errno(errno, "Failed to open %s: %m", p);

        r = button_suitable(b);
        if (r < 0)
                return log_warning_errno(r, "Failed to determine whether input device is relevant to us: %m");
        if (r == 0)
                return log_debug_errno(SYNTHETIC_ERRNO(EADDRNOTAVAIL),
                                       "Device %s does not expose keys or switches relevant to us, ignoring.",
                                       p);

        if (ioctl(b->fd, EVIOCGNAME(sizeof(name)), name) < 0) {
                r = log_error_errno(errno, "Failed to get input name: %m");
                goto fail;
        }

        (void) button_set_mask(b);

        r = sd_event_add_io(b->manager->event, &b->io_event_source, b->fd, EPOLLIN, button_dispatch, b);
        if (r < 0) {
                log_error_errno(r, "Failed to add button event: %m");
                goto fail;
        }

        log_info("Watching system buttons on /dev/input/%s (%s)", b->name, name);

        return 0;

fail:
        b->fd = safe_close(b->fd);
        return r;
}