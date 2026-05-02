static int manager_dispatch_notify_fd(sd_event_source *source, int fd, uint32_t revents, void *userdata) {
        Manager *m = userdata;
        ssize_t n;

        assert(m);
        assert(m->notify_fd == fd);

        if (revents != EPOLLIN) {
                log_warning("Got unexpected poll event for notify fd.");
                return 0;
        }

        for (;;) {
                char buf[4096];
                struct iovec iovec = {
                        .iov_base = buf,
                        .iov_len = sizeof(buf)-1,
                };

                union {
                        struct cmsghdr cmsghdr;
                        uint8_t buf[CMSG_SPACE(sizeof(struct ucred))];
                } control = {};

                struct msghdr msghdr = {
                        .msg_iov = &iovec,
                        .msg_iovlen = 1,
                        .msg_control = &control,
                        .msg_controllen = sizeof(control),
                };
                struct ucred *ucred;
                Unit *u;
                _cleanup_strv_free_ char **tags = NULL;

                n = recvmsg(m->notify_fd, &msghdr, MSG_DONTWAIT);
                if (n <= 0) {
                        if (n == 0)
                                return -EIO;

                        if (errno == EAGAIN || errno == EINTR)
                                break;

                        return -errno;
                }

                if (msghdr.msg_controllen < CMSG_LEN(sizeof(struct ucred)) ||
                    control.cmsghdr.cmsg_level != SOL_SOCKET ||
                    control.cmsghdr.cmsg_type != SCM_CREDENTIALS ||
                    control.cmsghdr.cmsg_len != CMSG_LEN(sizeof(struct ucred))) {
                        log_warning("Received notify message without credentials. Ignoring.");
                        continue;
                }

                ucred = (struct ucred*) CMSG_DATA(&control.cmsghdr);

                u = hashmap_get(m->watch_pids, LONG_TO_PTR(ucred->pid));
                if (!u) {
                        u = manager_get_unit_by_pid(m, ucred->pid);
                        if (!u) {
                                log_warning("Cannot find unit for notify message of PID "PID_FMT".", ucred->pid);
                                continue;
                        }
                }

                assert((size_t) n < sizeof(buf));
                buf[n] = 0;
                tags = strv_split(buf, "\n\r");
                if (!tags)
                        return log_oom();

                log_debug_unit(u->id, "Got notification message for unit %s", u->id);

                if (UNIT_VTABLE(u)->notify_message)
                        UNIT_VTABLE(u)->notify_message(u, ucred->pid, tags);
        }

        return 0;
}