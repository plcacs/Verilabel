static int dns_stream_complete(DnsStream *s, int error) {
        _cleanup_(dns_stream_unrefp) _unused_ DnsStream *ref = dns_stream_ref(s); /* Protect stream while we process it */

        assert(s);

#if ENABLE_DNS_OVER_TLS
        if (s->encrypted) {
                int r;

                r = dnstls_stream_shutdown(s, error);
                if (r != -EAGAIN)
                        dns_stream_stop(s);
        } else
#endif
                dns_stream_stop(s);

        if (s->complete)
                s->complete(s, error);
        else /* the default action if no completion function is set is to close the stream */
                dns_stream_unref(s);

        return 0;
}