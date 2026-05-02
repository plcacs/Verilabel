int dhcp6_option_append_ia(uint8_t **buf, size_t *buflen, const DHCP6IA *ia) {
        uint16_t len;
        uint8_t *ia_hdr;
        size_t iaid_offset, ia_buflen, ia_addrlen = 0;
        DHCP6Address *addr;
        int r;

        assert_return(buf, -EINVAL);
        assert_return(*buf, -EINVAL);
        assert_return(buflen, -EINVAL);
        assert_return(ia, -EINVAL);

        switch (ia->type) {
        case SD_DHCP6_OPTION_IA_NA:
                len = DHCP6_OPTION_IA_NA_LEN;
                iaid_offset = offsetof(DHCP6IA, ia_na);
                break;

        case SD_DHCP6_OPTION_IA_TA:
                len = DHCP6_OPTION_IA_TA_LEN;
                iaid_offset = offsetof(DHCP6IA, ia_ta);
                break;

        default:
                return -EINVAL;
        }

        if (*buflen < len)
                return -ENOBUFS;

        ia_hdr = *buf;
        ia_buflen = *buflen;

        *buf += sizeof(DHCP6Option);
        *buflen -= sizeof(DHCP6Option);

        memcpy(*buf, (char*) ia + iaid_offset, len);

        *buf += len;
        *buflen -= len;

        LIST_FOREACH(addresses, addr, ia->addresses) {
                r = option_append_hdr(buf, buflen, SD_DHCP6_OPTION_IAADDR,
                                      sizeof(addr->iaaddr));
                if (r < 0)
                        return r;

                memcpy(*buf, &addr->iaaddr, sizeof(addr->iaaddr));

                *buf += sizeof(addr->iaaddr);
                *buflen -= sizeof(addr->iaaddr);

                ia_addrlen += sizeof(DHCP6Option) + sizeof(addr->iaaddr);
        }

        r = option_append_hdr(&ia_hdr, &ia_buflen, ia->type, len + ia_addrlen);
        if (r < 0)
                return r;

        return 0;
}