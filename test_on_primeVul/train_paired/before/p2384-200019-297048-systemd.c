static void manager_invoke_notify_message(Manager *m, Unit *u, pid_t pid, const char *buf, size_t n, FDSet *fds) {
        _cleanup_strv_free_ char **tags = NULL;

        assert(m);
        assert(u);
        assert(buf);
        assert(n > 0);

        tags = strv_split(buf, "\n\r");
        if (!tags) {
                log_oom();
                return;
        }

        if (UNIT_VTABLE(u)->notify_message)
                UNIT_VTABLE(u)->notify_message(u, pid, tags, fds);
        else
                log_unit_debug(u, "Got notification message for unit. Ignoring.");
}