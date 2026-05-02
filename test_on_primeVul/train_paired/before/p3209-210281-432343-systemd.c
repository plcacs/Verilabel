static int on_stream_io(sd_event_source *es, int fd, uint32_t revents, void *userdata) {
        DnsStream *s = userdata;
        int r;

        assert(s);

#if ENABLE_DNS_OVER_TLS
        if (s->encrypted) {
                r = dnstls_stream_on_io(s, revents);
                if (r == DNSTLS_STREAM_CLOSED)
                        return 0;
                if (r == -EAGAIN)
                        return dns_stream_update_io(s);
                if (r < 0)
                        return dns_stream_complete(s, -r);

                r = dns_stream_update_io(s);
                if (r < 0)
                        return r;
        }
#endif

        /* only identify after connecting */
        if (s->tfo_salen == 0) {
                r = dns_stream_identify(s);
                if (r < 0)
                        return dns_stream_complete(s, -r);
        }

        if ((revents & EPOLLOUT) &&
            s->write_packet &&
            s->n_written < sizeof(s->write_size) + s->write_packet->size) {

                struct iovec iov[2];
                ssize_t ss;

                iov[0] = IOVEC_MAKE(&s->write_size, sizeof(s->write_size));
                iov[1] = IOVEC_MAKE(DNS_PACKET_DATA(s->write_packet), s->write_packet->size);

                IOVEC_INCREMENT(iov, 2, s->n_written);

                ss = dns_stream_writev(s, iov, 2, 0);
                if (ss < 0) {
                        if (!IN_SET(-ss, EINTR, EAGAIN))
                                return dns_stream_complete(s, -ss);
                } else
                        s->n_written += ss;

                /* Are we done? If so, disable the event source for EPOLLOUT */
                if (s->n_written >= sizeof(s->write_size) + s->write_packet->size) {
                        r = dns_stream_update_io(s);
                        if (r < 0)
                                return dns_stream_complete(s, -r);
                }
        }

        if ((revents & (EPOLLIN|EPOLLHUP|EPOLLRDHUP)) &&
            (!s->read_packet ||
             s->n_read < sizeof(s->read_size) + s->read_packet->size)) {

                if (s->n_read < sizeof(s->read_size)) {
                        ssize_t ss;

                        ss = dns_stream_read(s, (uint8_t*) &s->read_size + s->n_read, sizeof(s->read_size) - s->n_read);
                        if (ss < 0) {
                                if (!IN_SET(-ss, EINTR, EAGAIN))
                                        return dns_stream_complete(s, -ss);
                        } else if (ss == 0)
                                return dns_stream_complete(s, ECONNRESET);
                        else
                                s->n_read += ss;
                }

                if (s->n_read >= sizeof(s->read_size)) {

                        if (be16toh(s->read_size) < DNS_PACKET_HEADER_SIZE)
                                return dns_stream_complete(s, EBADMSG);

                        if (s->n_read < sizeof(s->read_size) + be16toh(s->read_size)) {
                                ssize_t ss;

                                if (!s->read_packet) {
                                        r = dns_packet_new(&s->read_packet, s->protocol, be16toh(s->read_size), DNS_PACKET_SIZE_MAX);
                                        if (r < 0)
                                                return dns_stream_complete(s, -r);

                                        s->read_packet->size = be16toh(s->read_size);
                                        s->read_packet->ipproto = IPPROTO_TCP;
                                        s->read_packet->family = s->peer.sa.sa_family;
                                        s->read_packet->ttl = s->ttl;
                                        s->read_packet->ifindex = s->ifindex;

                                        if (s->read_packet->family == AF_INET) {
                                                s->read_packet->sender.in = s->peer.in.sin_addr;
                                                s->read_packet->sender_port = be16toh(s->peer.in.sin_port);
                                                s->read_packet->destination.in = s->local.in.sin_addr;
                                                s->read_packet->destination_port = be16toh(s->local.in.sin_port);
                                        } else {
                                                assert(s->read_packet->family == AF_INET6);
                                                s->read_packet->sender.in6 = s->peer.in6.sin6_addr;
                                                s->read_packet->sender_port = be16toh(s->peer.in6.sin6_port);
                                                s->read_packet->destination.in6 = s->local.in6.sin6_addr;
                                                s->read_packet->destination_port = be16toh(s->local.in6.sin6_port);

                                                if (s->read_packet->ifindex == 0)
                                                        s->read_packet->ifindex = s->peer.in6.sin6_scope_id;
                                                if (s->read_packet->ifindex == 0)
                                                        s->read_packet->ifindex = s->local.in6.sin6_scope_id;
                                        }
                                }

                                ss = dns_stream_read(s,
                                          (uint8_t*) DNS_PACKET_DATA(s->read_packet) + s->n_read - sizeof(s->read_size),
                                          sizeof(s->read_size) + be16toh(s->read_size) - s->n_read);
                                if (ss < 0) {
                                        if (!IN_SET(-ss, EINTR, EAGAIN))
                                                return dns_stream_complete(s, -ss);
                                } else if (ss == 0)
                                        return dns_stream_complete(s, ECONNRESET);
                                else
                                        s->n_read += ss;
                        }

                        /* Are we done? If so, disable the event source for EPOLLIN */
                        if (s->n_read >= sizeof(s->read_size) + be16toh(s->read_size)) {
                                /* If there's a packet handler
                                 * installed, call that. Note that
                                 * this is optional... */
                                if (s->on_packet) {
                                        r = s->on_packet(s);
                                        if (r < 0)
                                                return r;
                                }

                                r = dns_stream_update_io(s);
                                if (r < 0)
                                        return dns_stream_complete(s, -r);
                        }
                }
        }

        if ((s->write_packet && s->n_written >= sizeof(s->write_size) + s->write_packet->size) &&
            (s->read_packet && s->n_read >= sizeof(s->read_size) + s->read_packet->size))
                return dns_stream_complete(s, 0);

        return 0;
}