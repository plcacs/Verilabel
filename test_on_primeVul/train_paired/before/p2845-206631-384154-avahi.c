static void dispatch_packet(AvahiServer *s, AvahiDnsPacket *p, const AvahiAddress *src_address, uint16_t port, const AvahiAddress *dst_address, AvahiIfIndex iface, int ttl) {
    AvahiInterface *i;
    int from_local_iface = 0;

    assert(s);
    assert(p);
    assert(src_address);
    assert(dst_address);
    assert(iface > 0);
    assert(src_address->proto == dst_address->proto);

    if (!(i = avahi_interface_monitor_get_interface(s->monitor, iface, src_address->proto)) ||
        !i->announcing) {
        avahi_log_warn("Received packet from invalid interface.");
        return;
    }

    if (avahi_address_is_ipv4_in_ipv6(src_address))
        /* This is an IPv4 address encapsulated in IPv6, so let's ignore it. */
        return;

    if (originates_from_local_legacy_unicast_socket(s, src_address, port))
        /* This originates from our local reflector, so let's ignore it */
        return;

    /* We don't want to reflect local traffic, so we check if this packet is generated locally. */
    if (s->config.enable_reflector)
        from_local_iface = originates_from_local_iface(s, iface, src_address, port);

    if (avahi_dns_packet_check_valid_multicast(p) < 0) {
        avahi_log_warn("Received invalid packet.");
        return;
    }

    if (avahi_dns_packet_is_query(p)) {
        int legacy_unicast = 0;

        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ARCOUNT) != 0) {
            avahi_log_warn("Invalid query packet.");
            return;
        }

        if (port != AVAHI_MDNS_PORT) {
            /* Legacy Unicast */

            if ((avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) != 0 ||
                 avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) != 0)) {
                avahi_log_warn("Invalid legacy unicast query packet.");
                return;
            }

            legacy_unicast = 1;
        }

        if (legacy_unicast)
            reflect_legacy_unicast_query_packet(s, p, i, src_address, port);

        handle_query_packet(s, p, i, src_address, port, legacy_unicast, from_local_iface);

    } else {
        char t[AVAHI_ADDRESS_STR_MAX];

        if (port != AVAHI_MDNS_PORT) {
            avahi_log_warn("Received response from host %s with invalid source port %u on interface '%s.%i'", avahi_address_snprint(t, sizeof(t), src_address), port, i->hardware->name, i->protocol);
            return;
        }

        if (ttl != 255 && s->config.check_response_ttl) {
            avahi_log_warn("Received response from host %s with invalid TTL %u on interface '%s.%i'.", avahi_address_snprint(t, sizeof(t), src_address), ttl, i->hardware->name, i->protocol);
            return;
        }

        if (!is_mdns_mcast_address(dst_address) &&
            !avahi_interface_address_on_link(i, src_address)) {

            avahi_log_warn("Received non-local response from host %s on interface '%s.%i'.", avahi_address_snprint(t, sizeof(t), src_address), i->hardware->name, i->protocol);
            return;
        }

        if (avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_QDCOUNT) != 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_ANCOUNT) == 0 ||
            avahi_dns_packet_get_field(p, AVAHI_DNS_FIELD_NSCOUNT) != 0) {

            avahi_log_warn("Invalid response packet from host %s.", avahi_address_snprint(t, sizeof(t), src_address));
            return;
        }

        handle_response_packet(s, p, i, src_address, from_local_iface);
    }
}