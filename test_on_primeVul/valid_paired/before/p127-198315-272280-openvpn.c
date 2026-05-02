multi_process_incoming_link(struct multi_context *m, struct multi_instance *instance, const unsigned int mpp_flags)
{
    struct gc_arena gc = gc_new();

    struct context *c;
    struct mroute_addr src, dest;
    unsigned int mroute_flags;
    struct multi_instance *mi;
    bool ret = true;
    bool floated = false;

    if (m->pending)
    {
        return true;
    }

    if (!instance)
    {
#ifdef MULTI_DEBUG_EVENT_LOOP
        printf("TCP/UDP -> TUN [%d]\n", BLEN(&m->top.c2.buf));
#endif
        multi_set_pending(m, multi_get_create_instance_udp(m, &floated));
    }
    else
    {
        multi_set_pending(m, instance);
    }

    if (m->pending)
    {
        set_prefix(m->pending);

        /* get instance context */
        c = &m->pending->context;

        if (!instance)
        {
            /* transfer packet pointer from top-level context buffer to instance */
            c->c2.buf = m->top.c2.buf;

            /* transfer from-addr from top-level context buffer to instance */
            if (!floated)
            {
                c->c2.from = m->top.c2.from;
            }
        }

        if (BLEN(&c->c2.buf) > 0)
        {
            struct link_socket_info *lsi;
            const uint8_t *orig_buf;

            /* decrypt in instance context */

            perf_push(PERF_PROC_IN_LINK);
            lsi = get_link_socket_info(c);
            orig_buf = c->c2.buf.data;
            if (process_incoming_link_part1(c, lsi, floated))
            {
                if (floated)
                {
                    multi_process_float(m, m->pending);
                }

                process_incoming_link_part2(c, lsi, orig_buf);
            }
            perf_pop();

            if (TUNNEL_TYPE(m->top.c1.tuntap) == DEV_TYPE_TUN)
            {
                /* extract packet source and dest addresses */
                mroute_flags = mroute_extract_addr_from_packet(&src,
                                                               &dest,
                                                               NULL,
                                                               NULL,
                                                               0,
                                                               &c->c2.to_tun,
                                                               DEV_TYPE_TUN);

                /* drop packet if extract failed */
                if (!(mroute_flags & MROUTE_EXTRACT_SUCCEEDED))
                {
                    c->c2.to_tun.len = 0;
                }
                /* make sure that source address is associated with this client */
                else if (multi_get_instance_by_virtual_addr(m, &src, true) != m->pending)
                {
                    /* IPv6 link-local address (fe80::xxx)? */
                    if ( (src.type & MR_ADDR_MASK) == MR_ADDR_IPV6
                         && IN6_IS_ADDR_LINKLOCAL(&src.v6.addr) )
                    {
                        /* do nothing, for now.  TODO: add address learning */
                    }
                    else
                    {
                        msg(D_MULTI_DROPPED, "MULTI: bad source address from client [%s], packet dropped",
                            mroute_addr_print(&src, &gc));
                    }
                    c->c2.to_tun.len = 0;
                }
                /* client-to-client communication enabled? */
                else if (m->enable_c2c)
                {
                    /* multicast? */
                    if (mroute_flags & MROUTE_EXTRACT_MCAST)
                    {
                        /* for now, treat multicast as broadcast */
                        multi_bcast(m, &c->c2.to_tun, m->pending, NULL, 0);
                    }
                    else /* possible client to client routing */
                    {
                        ASSERT(!(mroute_flags & MROUTE_EXTRACT_BCAST));
                        mi = multi_get_instance_by_virtual_addr(m, &dest, true);

                        /* if dest addr is a known client, route to it */
                        if (mi)
                        {
#ifdef ENABLE_PF
                            if (!pf_c2c_test(&c->c2.pf, c->c2.tls_multi,
                                             &mi->context.c2.pf,
                                             mi->context.c2.tls_multi,
                                             "tun_c2c"))
                            {
                                msg(D_PF_DROPPED, "PF: client -> client[%s] packet dropped by TUN packet filter",
                                    mi_prefix(mi));
                            }
                            else
#endif
                            {
                                multi_unicast(m, &c->c2.to_tun, mi);
                                register_activity(c, BLEN(&c->c2.to_tun));
                            }
                            c->c2.to_tun.len = 0;
                        }
                    }
                }
#ifdef ENABLE_PF
                if (c->c2.to_tun.len && !pf_addr_test(&c->c2.pf, c, &dest,
                                                      "tun_dest_addr"))
                {
                    msg(D_PF_DROPPED, "PF: client -> addr[%s] packet dropped by TUN packet filter",
                        mroute_addr_print_ex(&dest, MAPF_SHOW_ARP, &gc));
                    c->c2.to_tun.len = 0;
                }
#endif
            }
            else if (TUNNEL_TYPE(m->top.c1.tuntap) == DEV_TYPE_TAP)
            {
                uint16_t vid = 0;
#ifdef ENABLE_PF
                struct mroute_addr edest;
                mroute_addr_reset(&edest);
#endif

                if (m->top.options.vlan_tagging)
                {
                    if (vlan_is_tagged(&c->c2.to_tun))
                    {
                        /* Drop VLAN-tagged frame. */
                        msg(D_VLAN_DEBUG, "dropping incoming VLAN-tagged frame");
                        c->c2.to_tun.len = 0;
                    }
                    else
                    {
                        vid = c->options.vlan_pvid;
                    }
                }
                /* extract packet source and dest addresses */
                mroute_flags = mroute_extract_addr_from_packet(&src,
                                                               &dest,
                                                               NULL,
#ifdef ENABLE_PF
                                                               &edest,
#else
                                                               NULL,
#endif
                                                               vid,
                                                               &c->c2.to_tun,
                                                               DEV_TYPE_TAP);

                if (mroute_flags & MROUTE_EXTRACT_SUCCEEDED)
                {
                    if (multi_learn_addr(m, m->pending, &src, 0) == m->pending)
                    {
                        /* check for broadcast */
                        if (m->enable_c2c)
                        {
                            if (mroute_flags & (MROUTE_EXTRACT_BCAST|MROUTE_EXTRACT_MCAST))
                            {
                                multi_bcast(m, &c->c2.to_tun, m->pending, NULL,
                                            vid);
                            }
                            else /* try client-to-client routing */
                            {
                                mi = multi_get_instance_by_virtual_addr(m, &dest, false);

                                /* if dest addr is a known client, route to it */
                                if (mi)
                                {
#ifdef ENABLE_PF
                                    if (!pf_c2c_test(&c->c2.pf, c->c2.tls_multi,
                                                     &mi->context.c2.pf,
                                                     mi->context.c2.tls_multi,
                                                     "tap_c2c"))
                                    {
                                        msg(D_PF_DROPPED, "PF: client -> client[%s] packet dropped by TAP packet filter",
                                            mi_prefix(mi));
                                    }
                                    else
#endif
                                    {
                                        multi_unicast(m, &c->c2.to_tun, mi);
                                        register_activity(c, BLEN(&c->c2.to_tun));
                                    }
                                    c->c2.to_tun.len = 0;
                                }
                            }
                        }
#ifdef ENABLE_PF
                        if (c->c2.to_tun.len && !pf_addr_test(&c->c2.pf, c,
                                                              &edest,
                                                              "tap_dest_addr"))
                        {
                            msg(D_PF_DROPPED, "PF: client -> addr[%s] packet dropped by TAP packet filter",
                                mroute_addr_print_ex(&edest, MAPF_SHOW_ARP, &gc));
                            c->c2.to_tun.len = 0;
                        }
#endif
                    }
                    else
                    {
                        msg(D_MULTI_DROPPED, "MULTI: bad source address from client [%s], packet dropped",
                            mroute_addr_print(&src, &gc));
                        c->c2.to_tun.len = 0;
                    }
                }
                else
                {
                    c->c2.to_tun.len = 0;
                }
            }
        }

        /* postprocess and set wakeup */
        ret = multi_process_post(m, m->pending, mpp_flags);

        clear_prefix();
    }

    gc_free(&gc);
    return ret;
}