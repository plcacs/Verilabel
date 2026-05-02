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
                bool found = false;

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

                assert((size_t) n < sizeof(buf));
                buf[n] = 0;

                u = manager_get_unit_by_pid(m, ucred->pid);
                if (u) {
                        manager_invoke_notify_message(m, u, ucred->pid, buf, n);
                        found = true;
                }

                u = hashmap_get(m->watch_pids1, LONG_TO_PTR(ucred->pid));
                if (u) {
                        manager_invoke_notify_message(m, u, ucred->pid, buf, n);
                        found = true;
                }

                u = hashmap_get(m->watch_pids2, LONG_TO_PTR(ucred->pid));
                if (u) {
                        manager_invoke_notify_message(m, u, ucred->pid, buf, n);
                        found = true;
                }

                if (!found)
                        log_warning("Cannot find unit for notify message of PID "PID_FMT".", ucred->pid);
        }

        return 0;
}