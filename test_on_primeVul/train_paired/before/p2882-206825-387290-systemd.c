static int server_process_entry(
                Server *s,
                const void *buffer, size_t *remaining,
                ClientContext *context,
                const struct ucred *ucred,
                const struct timeval *tv,
                const char *label, size_t label_len) {

        /* Process a single entry from a native message. Returns 0 if nothing special happened and the message
         * processing should continue, and a negative or positive value otherwise.
         *
         * Note that *remaining is altered on both success and failure. */

        size_t n = 0, j, tn = (size_t) -1, m = 0, entry_size = 0;
        char *identifier = NULL, *message = NULL;
        struct iovec *iovec = NULL;
        int priority = LOG_INFO;
        pid_t object_pid = 0;
        const char *p;
        int r = 0;

        p = buffer;

        while (*remaining > 0) {
                const char *e, *q;

                e = memchr(p, '\n', *remaining);

                if (!e) {
                        /* Trailing noise, let's ignore it, and flush what we collected */
                        log_debug("Received message with trailing noise, ignoring.");
                        r = 1; /* finish processing of the message */
                        break;
                }

                if (e == p) {
                        /* Entry separator */
                        *remaining -= 1;
                        break;
                }

                if (IN_SET(*p, '.', '#')) {
                        /* Ignore control commands for now, and
                         * comments too. */
                        *remaining -= (e - p) + 1;
                        p = e + 1;
                        continue;
                }

                /* A property follows */

                /* n existing properties, 1 new, +1 for _TRANSPORT */
                if (!GREEDY_REALLOC(iovec, m,
                                    n + 2 +
                                    N_IOVEC_META_FIELDS + N_IOVEC_OBJECT_FIELDS +
                                    client_context_extra_fields_n_iovec(context))) {
                        r = log_oom();
                        break;
                }

                q = memchr(p, '=', e - p);
                if (q) {
                        if (journal_field_valid(p, q - p, false)) {
                                size_t l;

                                l = e - p;

                                /* If the field name starts with an underscore, skip the variable, since that indicates
                                 * a trusted field */
                                iovec[n++] = IOVEC_MAKE((char*) p, l);
                                entry_size += l;

                                server_process_entry_meta(p, l, ucred,
                                                          &priority,
                                                          &identifier,
                                                          &message,
                                                          &object_pid);
                        }

                        *remaining -= (e - p) + 1;
                        p = e + 1;
                        continue;
                } else {
                        uint64_t l;
                        char *k;

                        if (*remaining < e - p + 1 + sizeof(uint64_t) + 1) {
                                log_debug("Failed to parse message, ignoring.");
                                break;
                        }

                        l = unaligned_read_le64(e + 1);

                        if (l > DATA_SIZE_MAX) {
                                log_debug("Received binary data block of %"PRIu64" bytes is too large, ignoring.", l);
                                break;
                        }

                        if ((uint64_t) *remaining < e - p + 1 + sizeof(uint64_t) + l + 1 ||
                            e[1+sizeof(uint64_t)+l] != '\n') {
                                log_debug("Failed to parse message, ignoring.");
                                break;
                        }

                        k = malloc((e - p) + 1 + l);
                        if (!k) {
                                log_oom();
                                break;
                        }

                        memcpy(k, p, e - p);
                        k[e - p] = '=';
                        memcpy(k + (e - p) + 1, e + 1 + sizeof(uint64_t), l);

                        if (journal_field_valid(p, e - p, false)) {
                                iovec[n] = IOVEC_MAKE(k, (e - p) + 1 + l);
                                entry_size += iovec[n].iov_len;
                                n++;

                                server_process_entry_meta(k, (e - p) + 1 + l, ucred,
                                                          &priority,
                                                          &identifier,
                                                          &message,
                                                          &object_pid);
                        } else
                                free(k);

                        *remaining -= (e - p) + 1 + sizeof(uint64_t) + l + 1;
                        p = e + 1 + sizeof(uint64_t) + l + 1;
                }
        }

        if (n <= 0) {
                r = 1;
                goto finish;
        }

        if (!client_context_test_priority(context, priority)) {
                r = 0;
                goto finish;
        }

        tn = n++;
        iovec[tn] = IOVEC_MAKE_STRING("_TRANSPORT=journal");
        entry_size += STRLEN("_TRANSPORT=journal");

        if (entry_size + n + 1 > ENTRY_SIZE_MAX) { /* data + separators + trailer */
                log_debug("Entry is too big with %zu properties and %zu bytes, ignoring.", n, entry_size);
                goto finish;
        }

        if (message) {
                if (s->forward_to_syslog)
                        server_forward_syslog(s, syslog_fixup_facility(priority), identifier, message, ucred, tv);

                if (s->forward_to_kmsg)
                        server_forward_kmsg(s, priority, identifier, message, ucred);

                if (s->forward_to_console)
                        server_forward_console(s, priority, identifier, message, ucred);

                if (s->forward_to_wall)
                        server_forward_wall(s, priority, identifier, message, ucred);
        }

        server_dispatch_message(s, iovec, n, m, context, tv, priority, object_pid);

finish:
        for (j = 0; j < n; j++)  {
                if (j == tn)
                        continue;

                if (iovec[j].iov_base < buffer ||
                    (const char*) iovec[j].iov_base >= p + *remaining)
                        free(iovec[j].iov_base);
        }

        free(iovec);
        free(identifier);
        free(message);

        return r;
}