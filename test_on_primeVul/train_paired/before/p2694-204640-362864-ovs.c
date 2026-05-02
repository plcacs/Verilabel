lldp_decode(struct lldpd *cfg OVS_UNUSED, char *frame, int s,
            struct lldpd_hardware *hardware, struct lldpd_chassis **newchassis,
            struct lldpd_port **newport)
{
    struct lldpd_chassis *chassis;
    struct lldpd_port *port;
    const struct eth_addr lldpaddr = LLDP_MULTICAST_ADDR;
    const char dot1[] = LLDP_TLV_ORG_DOT1;
    const char dot3[] = LLDP_TLV_ORG_DOT3;
    const char med[] = LLDP_TLV_ORG_MED;
    const char avaya_oid[] = LLDP_TLV_ORG_AVAYA;
    const char dcbx[] = LLDP_TLV_ORG_DCBX;
    char orgid[3];
    int length, af;
    bool gotend = false;
    bool ttl_received = false;
    int tlv_size, tlv_type, tlv_subtype, tlv_count = 0;
    u_int8_t *pos, *tlv;
    void *b;
    struct lldpd_aa_isid_vlan_maps_tlv *isid_vlan_map = NULL;
    u_int8_t msg_auth_digest[LLDP_TLV_AA_ISID_VLAN_DIGEST_LENGTH];
    struct lldpd_mgmt *mgmt;
    u_int8_t addr_str_length, addr_str_buffer[32];
    u_int8_t addr_family, addr_length, *addr_ptr, iface_subtype;
    u_int32_t iface_number, iface;

    VLOG_DBG("receive LLDP PDU on %s", hardware->h_ifname);

    chassis = xzalloc(sizeof *chassis);
    ovs_list_init(&chassis->c_mgmt);

    port = xzalloc(sizeof *port);
    ovs_list_init(&port->p_isid_vlan_maps);

    length = s;
    pos = (u_int8_t*) frame;

    if (length < 2 * ETH_ADDR_LEN + sizeof(u_int16_t)) {
        VLOG_WARN("too short frame received on %s", hardware->h_ifname);
        goto malformed;
    }
    if (PEEK_CMP(&lldpaddr, ETH_ADDR_LEN) != 0) {
        VLOG_INFO("frame not targeted at LLDP multicast address "
                  "received on %s", hardware->h_ifname);
        goto malformed;
    }

    PEEK_DISCARD(ETH_ADDR_LEN); /* Skip source address */
    if (PEEK_UINT16 != ETHERTYPE_LLDP) {
        VLOG_INFO("non LLDP frame received on %s", hardware->h_ifname);
        goto malformed;
    }

    while (length && !gotend) {
        if (length < 2) {
            VLOG_WARN("tlv header too short received on %s",
                      hardware->h_ifname);
            goto malformed;
        }
        tlv_size = PEEK_UINT16;
        tlv_type = tlv_size >> 9;
        tlv_size = tlv_size & 0x1ff;
        (void) PEEK_SAVE(tlv);
        if (length < tlv_size) {
            VLOG_WARN("frame too short for tlv received on %s",
                      hardware->h_ifname);
            goto malformed;
        }
        /* Check order for mandatory TLVs */
        tlv_count++;
        switch (tlv_type) {
        case LLDP_TLV_CHASSIS_ID:
            if (tlv_count != 1) {
                VLOG_WARN("first TLV should be a chassis ID on %s, not %d",
                          hardware->h_ifname, tlv_type);
                goto malformed;
            }
            break;
        case LLDP_TLV_PORT_ID:
            if (tlv_count != 2) {
                VLOG_WARN("second TLV should be a port ID on %s, not %d",
                          hardware->h_ifname, tlv_type);
                goto malformed;
            }
            break;
        case LLDP_TLV_TTL:
            if (tlv_count != 3) {
                VLOG_WARN("third TLV should be a TTL on %s, not %d",
                          hardware->h_ifname, tlv_type);
                goto malformed;
            }
            break;
        }

        switch (tlv_type) {
        case LLDP_TLV_END:
            if (tlv_size != 0) {
                VLOG_WARN("lldp end received with size not null on %s",
                          hardware->h_ifname);
                goto malformed;
            }
            if (length) {
                VLOG_DBG("extra data after lldp end on %s",
                         hardware->h_ifname);
            }
            gotend = true;
            break;

        case LLDP_TLV_CHASSIS_ID:
        case LLDP_TLV_PORT_ID:
            CHECK_TLV_SIZE(2, "Port/Chassis Id");
            CHECK_TLV_MAX_SIZE(256, "Port/Chassis Id");
            tlv_subtype = PEEK_UINT8;
            if (tlv_subtype == 0 || tlv_subtype > 7) {
                VLOG_WARN("unknown subtype for tlv id received on %s",
                          hardware->h_ifname);
                goto malformed;
            }
            b = xzalloc(tlv_size - 1);
            PEEK_BYTES(b, tlv_size - 1);
            if (tlv_type == LLDP_TLV_PORT_ID) {
                if (port->p_id != NULL) {
                    VLOG_WARN("Port ID TLV received twice on %s",
                              hardware->h_ifname);
                    free(b);
                    goto malformed;
                }
                port->p_id_subtype = tlv_subtype;
                port->p_id = b;
                port->p_id_len = tlv_size - 1;
            } else {
                if (chassis->c_id != NULL) {
                    VLOG_WARN("Chassis ID TLV received twice on %s",
                              hardware->h_ifname);
                    free(b);
                    goto malformed;
                }
                chassis->c_id_subtype = tlv_subtype;
                chassis->c_id = b;
                chassis->c_id_len = tlv_size - 1;
            }
            break;

        case LLDP_TLV_TTL:
            if (ttl_received) {
                VLOG_WARN("TTL TLV received twice on %s",
                          hardware->h_ifname);
                goto malformed;
            }
            CHECK_TLV_SIZE(2, "TTL");
            chassis->c_ttl = PEEK_UINT16;
            ttl_received = true;
            break;

        case LLDP_TLV_PORT_DESCR:
        case LLDP_TLV_SYSTEM_NAME:
        case LLDP_TLV_SYSTEM_DESCR:
            if (tlv_size < 1) {
                VLOG_DBG("empty tlv received on %s", hardware->h_ifname);
                break;
            }
            b = xzalloc(tlv_size + 1);
            PEEK_BYTES(b, tlv_size);
            if (tlv_type == LLDP_TLV_PORT_DESCR) {
                port->p_descr = b;
            } else if (tlv_type == LLDP_TLV_SYSTEM_NAME) {
                chassis->c_name = b;
            } else {
                chassis->c_descr = b;
            }
            break;

        case LLDP_TLV_SYSTEM_CAP:
            CHECK_TLV_SIZE(4, "System capabilities");
            chassis->c_cap_available = PEEK_UINT16;
            chassis->c_cap_enabled = PEEK_UINT16;
            break;

        case LLDP_TLV_MGMT_ADDR:
            CHECK_TLV_SIZE(1, "Management address");
            addr_str_length = PEEK_UINT8;
            if (addr_str_length > sizeof(addr_str_buffer)) {
                VLOG_WARN("too large management address on %s",
                          hardware->h_ifname);
                goto malformed;
            }
            CHECK_TLV_SIZE(1 + addr_str_length, "Management address");
            PEEK_BYTES(addr_str_buffer, addr_str_length);
            addr_length = addr_str_length - 1;
            addr_family = addr_str_buffer[0];
            addr_ptr = &addr_str_buffer[1];
            CHECK_TLV_SIZE(1 + addr_str_length + 5, "Management address");
            iface_subtype = PEEK_UINT8;
            iface_number = PEEK_UINT32;

            af = lldpd_af_from_lldp_proto(addr_family);
            if (af == LLDPD_AF_UNSPEC) {
                break;
            }
            iface = iface_subtype == LLDP_MGMT_IFACE_IFINDEX ?
                iface_number : 0;
            mgmt = lldpd_alloc_mgmt(af, addr_ptr, addr_length, iface);
            if (mgmt == NULL) {
                VLOG_WARN("unable to allocate memory for management address");
                goto malformed;
            }
            ovs_list_push_back(&chassis->c_mgmt, &mgmt->m_entries);
            break;

        case LLDP_TLV_ORG:
            CHECK_TLV_SIZE(1 + sizeof orgid, "Organisational");
            PEEK_BYTES(orgid, sizeof orgid);
            tlv_subtype = PEEK_UINT8;
            if (memcmp(dot1, orgid, sizeof orgid) == 0) {
                hardware->h_rx_unrecognized_cnt++;
            } else if (memcmp(dot3, orgid, sizeof orgid) == 0) {
                hardware->h_rx_unrecognized_cnt++;
            } else if (memcmp(med, orgid, sizeof orgid) == 0) {
                /* LLDP-MED */
                hardware->h_rx_unrecognized_cnt++;
            } else if (memcmp(avaya_oid, orgid, sizeof orgid) == 0) {
                u_int32_t aa_element_dword;
                u_int16_t aa_system_id_word;
                u_int16_t aa_status_vlan_word;
                u_int8_t aa_element_state;
                unsigned short num_mappings;

                switch(tlv_subtype) {
                case LLDP_TLV_AA_ELEMENT_SUBTYPE:
                    PEEK_BYTES(&msg_auth_digest, sizeof msg_auth_digest);

                    aa_element_dword = PEEK_UINT32;

                    /* Type is first 6 most-significant bits of
                     * aa_element_dword */
                    port->p_element.type = aa_element_dword >> 26;

                    /* State is 6 most significant bits of aa_element_dword */
                    aa_element_state = (aa_element_dword >> 20) & 0x3F;

                    /* vlan tagging requirement is the bit 1(left to right)
                     * of the 6 bits state (1 based) */
                    port->p_element.vlan_tagging =
                        (aa_element_state >> 5) & 0x1;

                    /* Automatic provision mode is the bit 2/3(left to right)
                     * of the 6 bits state (1 based) */
                    port->p_element.auto_prov_mode =
                        (aa_element_state >> 3) & 0x3;

                    /* mgmt_vlan is the 12 bits of aa_element_dword from
                     * bit 12 */
                    port->p_element.mgmt_vlan =
                        (aa_element_dword >> 8) & 0xFFF;
                    VLOG_INFO("Element type: %X, vlan tagging %X, "
                              "auto prov mode %x, Mgmt vlan: %X",
                              port->p_element.type,
                              port->p_element.vlan_tagging,
                              port->p_element.auto_prov_mode,
                              port->p_element.mgmt_vlan);

                    PEEK_BYTES(&port->p_element.system_id.system_mac,
                               sizeof port->p_element.system_id.system_mac);
                    VLOG_INFO("System mac: "ETH_ADDR_FMT,
                        ETH_ADDR_ARGS(port->p_element.system_id.system_mac));
                    aa_system_id_word = PEEK_UINT16;
                    port->p_element.system_id.conn_type =
                        aa_system_id_word >> 13;
                    port->p_element.system_id.rsvd = aa_system_id_word &
                        0x03FF;
                    PEEK_BYTES(&port->p_element.system_id.rsvd2,
                               sizeof port->p_element.system_id.rsvd2);
                    break;

                case LLDP_TLV_AA_ISID_VLAN_ASGNS_SUBTYPE:
                    PEEK_BYTES(&msg_auth_digest, sizeof msg_auth_digest);

                    /* Subtract off tlv type and length (2Bytes) + OUI (3B) +
                     * Subtype (1B) + MSG DIGEST (32B).
                     */
                    num_mappings = tlv_size - 4 -
                        LLDP_TLV_AA_ISID_VLAN_DIGEST_LENGTH;
                    if (num_mappings % 5 != 0) {
                        VLOG_INFO("malformed vlan-isid mappings tlv received");
                        goto malformed;
                    }

                    num_mappings /= 5; /* Each mapping is 5 Bytes */
                    for(; num_mappings > 0; num_mappings--) {
                        uint8_t isid[3];

                        isid_vlan_map = xzalloc(sizeof *isid_vlan_map);
                        aa_status_vlan_word = PEEK_UINT16;

                        /* Status is first 4 most-significant bits. */
                        isid_vlan_map->isid_vlan_data.status =
                            aa_status_vlan_word >> 12;

                        /* Vlan is last 12 bits */
                        isid_vlan_map->isid_vlan_data.vlan =
                            aa_status_vlan_word & 0x0FFF;
                        PEEK_BYTES(isid, 3);
                        isid_vlan_map->isid_vlan_data.isid =
                            (isid[0] << 16) | (isid[1] << 8) | isid[2];
                        ovs_list_push_back(&port->p_isid_vlan_maps,
                                       &isid_vlan_map->m_entries);
                        isid_vlan_map = NULL;
                    }
                    break;

                default:
                    hardware->h_rx_unrecognized_cnt++;
                    VLOG_INFO("Unrecogised tlv subtype received");
                    break;
                }
            } else if (memcmp(dcbx, orgid, sizeof orgid) == 0) {
                VLOG_DBG("unsupported DCBX tlv received on %s "
                         "- ignore", hardware->h_ifname);
                hardware->h_rx_unrecognized_cnt++;
            } else {
                VLOG_INFO("unknown org tlv [%02x:%02x:%02x] received "
                          "on %s", orgid[0], orgid[1], orgid[2],
                          hardware->h_ifname);
                hardware->h_rx_unrecognized_cnt++;
            }
            break;
        default:
            VLOG_WARN("unknown tlv (%d) received on %s",
                      tlv_type,
                      hardware->h_ifname);
            hardware->h_rx_unrecognized_cnt++;
            goto malformed;
        }
        if (pos > tlv + tlv_size) {
            VLOG_WARN("BUG: already past TLV!");
            goto malformed;
        }
        PEEK_DISCARD(tlv + tlv_size - pos);
    }

    /* Some random check */
    if (!chassis->c_id || !port->p_id || !ttl_received || !gotend) {
        VLOG_WARN("some mandatory tlv are missing for frame received "
                  "on %s", hardware->h_ifname);
        goto malformed;
    }
    *newchassis = chassis;
    *newport = port;
    return 1;

malformed:
    lldpd_chassis_cleanup(chassis, true);
    lldpd_port_cleanup(port, true);
    free(port);
    return -1;
}