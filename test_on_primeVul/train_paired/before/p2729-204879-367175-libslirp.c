void ip6_input(struct mbuf *m)
{
    struct ip6 *ip6;
    Slirp *slirp = m->slirp;

    if (!slirp->in6_enabled) {
        goto bad;
    }

    DEBUG_CALL("ip6_input");
    DEBUG_ARG("m = %p", m);
    DEBUG_ARG("m_len = %d", m->m_len);

    if (m->m_len < sizeof(struct ip6)) {
        goto bad;
    }

    ip6 = mtod(m, struct ip6 *);

    if (ip6->ip_v != IP6VERSION) {
        goto bad;
    }

    if (ntohs(ip6->ip_pl) > slirp->if_mtu) {
        icmp6_send_error(m, ICMP6_TOOBIG, 0);
        goto bad;
    }

    /* check ip_ttl for a correct ICMP reply */
    if (ip6->ip_hl == 0) {
        icmp6_send_error(m, ICMP6_TIMXCEED, ICMP6_TIMXCEED_INTRANS);
        goto bad;
    }

    /*
     * Switch out to protocol's input routine.
     */
    switch (ip6->ip_nh) {
    case IPPROTO_TCP:
        NTOHS(ip6->ip_pl);
        tcp_input(m, sizeof(struct ip6), (struct socket *)NULL, AF_INET6);
        break;
    case IPPROTO_UDP:
        udp6_input(m);
        break;
    case IPPROTO_ICMPV6:
        icmp6_input(m);
        break;
    default:
        m_free(m);
    }
    return;
bad:
    m_free(m);
}