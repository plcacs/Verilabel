dissect_tcp(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
    guint8  th_off_x2; /* combines th_off and th_x2 */
    guint16 th_sum;
    guint32 th_urp;
    proto_tree *tcp_tree = NULL, *field_tree = NULL;
    proto_item *ti = NULL, *tf, *hidden_item;
    proto_item *options_item;
    proto_tree *options_tree;
    int        offset = 0;
    const char *flags_str, *flags_str_first_letter;
    guint      optlen;
    guint32    nxtseq = 0;
    guint      reported_len;
    vec_t      cksum_vec[4];
    guint32    phdr[2];
    guint16    computed_cksum;
    guint16    real_window;
    guint      captured_length_remaining;
    gboolean   desegment_ok;
    struct tcpinfo tcpinfo;
    struct tcpheader *tcph;
    proto_item *tf_syn = NULL, *tf_fin = NULL, *tf_rst = NULL, *scaled_pi;
    conversation_t *conv=NULL, *other_conv;
    guint32 save_last_frame = 0;
    struct tcp_analysis *tcpd=NULL;
    struct tcp_per_packet_data_t *tcppd=NULL;
    proto_item *item;
    proto_tree *checksum_tree;
    gboolean    icmp_ip = FALSE;

    tcph = wmem_new0(wmem_packet_scope(), struct tcpheader);
    tcph->th_sport = tvb_get_ntohs(tvb, offset);
    tcph->th_dport = tvb_get_ntohs(tvb, offset + 2);
    copy_address_shallow(&tcph->ip_src, &pinfo->src);
    copy_address_shallow(&tcph->ip_dst, &pinfo->dst);

    col_set_str(pinfo->cinfo, COL_PROTOCOL, "TCP");
    col_clear(pinfo->cinfo, COL_INFO);
    col_append_ports(pinfo->cinfo, COL_INFO, PT_TCP, tcph->th_sport, tcph->th_dport);

    if (tree) {
        ti = proto_tree_add_item(tree, proto_tcp, tvb, 0, -1, ENC_NA);
        if (tcp_summary_in_tree) {
            proto_item_append_text(ti, ", Src Port: %s, Dst Port: %s",
                    port_with_resolution_to_str(wmem_packet_scope(), PT_TCP, tcph->th_sport),
                    port_with_resolution_to_str(wmem_packet_scope(), PT_TCP, tcph->th_dport));
        }
        tcp_tree = proto_item_add_subtree(ti, ett_tcp);
        p_add_proto_data(pinfo->pool, pinfo, proto_tcp, pinfo->curr_layer_num, tcp_tree);

        proto_tree_add_item(tcp_tree, hf_tcp_srcport, tvb, offset, 2, ENC_BIG_ENDIAN);
        proto_tree_add_item(tcp_tree, hf_tcp_dstport, tvb, offset + 2, 2, ENC_BIG_ENDIAN);
        hidden_item = proto_tree_add_item(tcp_tree, hf_tcp_port, tvb, offset, 2, ENC_BIG_ENDIAN);
        PROTO_ITEM_SET_HIDDEN(hidden_item);
        hidden_item = proto_tree_add_item(tcp_tree, hf_tcp_port, tvb, offset + 2, 2, ENC_BIG_ENDIAN);
        PROTO_ITEM_SET_HIDDEN(hidden_item);

        /*  If we're dissecting the headers of a TCP packet in an ICMP packet
         *  then go ahead and put the sequence numbers in the tree now (because
         *  they won't be put in later because the ICMP packet only contains up
         *  to the sequence number).
         *  We should only need to do this for IPv4 since IPv6 will hopefully
         *  carry enough TCP payload for this dissector to put the sequence
         *  numbers in via the regular code path.
         */
        {
            wmem_list_frame_t *frame;
            frame = wmem_list_frame_prev(wmem_list_tail(pinfo->layers));
            if (proto_ip == (gint) GPOINTER_TO_UINT(wmem_list_frame_data(frame))) {
                frame = wmem_list_frame_prev(frame);
                if (proto_icmp == (gint) GPOINTER_TO_UINT(wmem_list_frame_data(frame))) {
                    proto_tree_add_item(tcp_tree, hf_tcp_seq, tvb, offset + 4, 4, ENC_BIG_ENDIAN);
                    icmp_ip = TRUE;
                }
            }
        }
    }

    /* Set the source and destination port numbers as soon as we get them,
       so that they're available to the "Follow TCP Stream" code even if
       we throw an exception dissecting the rest of the TCP header. */
    pinfo->ptype = PT_TCP;
    pinfo->srcport = tcph->th_sport;
    pinfo->destport = tcph->th_dport;

    p_add_proto_data(pinfo->pool, pinfo, hf_tcp_srcport, pinfo->curr_layer_num, GUINT_TO_POINTER(tcph->th_sport));
    p_add_proto_data(pinfo->pool, pinfo, hf_tcp_dstport, pinfo->curr_layer_num, GUINT_TO_POINTER(tcph->th_dport));

    tcph->th_rawseq = tvb_get_ntohl(tvb, offset + 4);
    tcph->th_seq = tcph->th_rawseq;
    tcph->th_ack = tvb_get_ntohl(tvb, offset + 8);
    th_off_x2 = tvb_get_guint8(tvb, offset + 12);
    tcpinfo.flags = tcph->th_flags = tvb_get_ntohs(tvb, offset + 12) & TH_MASK;
    tcph->th_win = tvb_get_ntohs(tvb, offset + 14);
    real_window = tcph->th_win;
    tcph->th_hlen = hi_nibble(th_off_x2) * 4;  /* TCP header length, in bytes */

    /* find(or create if needed) the conversation for this tcp session
     * This is a slight deviation from find_or_create_conversation so it's
     * done manually.  This is done to save the last frame of the conversation
     * in case a new conversation is found and the previous conversation needs
     * to be adjusted,
     */
    if((conv = find_conversation_pinfo(pinfo, 0)) != NULL) {
        /* Update how far the conversation reaches */
        if (pinfo->num > conv->last_frame) {
            save_last_frame = conv->last_frame;
            conv->last_frame = pinfo->num;
        }
    }
    else {
        conv = conversation_new(pinfo->num, &pinfo->src,
                     &pinfo->dst, ENDPOINT_TCP,
                     pinfo->srcport, pinfo->destport, 0);
    }
    tcpd=get_tcp_conversation_data(conv,pinfo);

    /* If this is a SYN packet, then check if its seq-nr is different
     * from the base_seq of the retrieved conversation. If this is the
     * case, create a new conversation with the same addresses and ports
     * and set the TA_PORTS_REUSED flag. If the seq-nr is the same as
     * the base_seq, then do nothing so it will be marked as a retrans-
     * mission later.
     * XXX - Is this affected by MPTCP which can use multiple SYNs?
     */
    if(tcpd && ((tcph->th_flags&(TH_SYN|TH_ACK))==TH_SYN) &&
       (tcpd->fwd->static_flags & TCP_S_BASE_SEQ_SET) &&
       (tcph->th_seq!=tcpd->fwd->base_seq) ) {
        if (!(pinfo->fd->visited)) {
            /* Reset the last frame seen in the conversation */
            if (save_last_frame > 0)
                conv->last_frame = save_last_frame;

            conv=conversation_new(pinfo->num, &pinfo->src, &pinfo->dst, ENDPOINT_TCP, pinfo->srcport, pinfo->destport, 0);
            tcpd=get_tcp_conversation_data(conv,pinfo);
        }
        if(!tcpd->ta)
            tcp_analyze_get_acked_struct(pinfo->num, tcph->th_seq, tcph->th_ack, TRUE, tcpd);
        tcpd->ta->flags|=TCP_A_REUSED_PORTS;
    }
    /* If this is a SYN/ACK packet, then check if its seq-nr is different
     * from the base_seq of the retrieved conversation. If this is the
     * case, try to find a conversation with the same addresses and ports
     * and set the TA_PORTS_REUSED flag. If the seq-nr is the same as
     * the base_seq, then do nothing so it will be marked as a retrans-
     * mission later.
     * XXX - Is this affected by MPTCP which can use multiple SYNs?
     */
    if(tcpd && ((tcph->th_flags&(TH_SYN|TH_ACK))==(TH_SYN|TH_ACK)) &&
       (tcpd->fwd->static_flags & TCP_S_BASE_SEQ_SET) &&
       (tcph->th_seq!=tcpd->fwd->base_seq) ) {
        if (!(pinfo->fd->visited)) {
            /* Reset the last frame seen in the conversation */
            if (save_last_frame > 0)
                conv->last_frame = save_last_frame;
        }

        other_conv = find_conversation(pinfo->num, &pinfo->dst, &pinfo->src, ENDPOINT_TCP, pinfo->destport, pinfo->srcport, 0);
        if (other_conv != NULL)
        {
            conv = other_conv;
            tcpd=get_tcp_conversation_data(conv,pinfo);
        }

        if(!tcpd->ta)
            tcp_analyze_get_acked_struct(pinfo->num, tcph->th_seq, tcph->th_ack, TRUE, tcpd);
        tcpd->ta->flags|=TCP_A_REUSED_PORTS;
    }

    if (tcpd) {
        item = proto_tree_add_uint(tcp_tree, hf_tcp_stream, tvb, offset, 0, tcpd->stream);
        PROTO_ITEM_SET_GENERATED(item);

        /* Copy the stream index into the header as well to make it available
         * to tap listeners.
         */
        tcph->th_stream = tcpd->stream;
    }

    /* Do we need to calculate timestamps relative to the tcp-stream? */
    if (tcp_calculate_ts) {
        tcppd = (struct tcp_per_packet_data_t *)p_get_proto_data(wmem_file_scope(), pinfo, proto_tcp, pinfo->curr_layer_num);

        /*
         * Calculate the timestamps relative to this conversation (but only on the
         * first run when frames are accessed sequentially)
         */
        if (!(pinfo->fd->visited))
            tcp_calculate_timestamps(pinfo, tcpd, tcppd);
    }

    /*
     * If we've been handed an IP fragment, we don't know how big the TCP
     * segment is, so don't do anything that requires that we know that.
     *
     * The same applies if we're part of an error packet.  (XXX - if the
     * ICMP and ICMPv6 dissectors could set a "this is how big the IP
     * header says it is" length in the tvbuff, we could use that; such
     * a length might also be useful for handling packets where the IP
     * length is bigger than the actual data available in the frame; the
     * dissectors should trust that length, and then throw a
     * ReportedBoundsError exception when they go past the end of the frame.)
     *
     * We also can't determine the segment length if the reported length
     * of the TCP packet is less than the TCP header length.
     */
    reported_len = tvb_reported_length(tvb);

    if (!pinfo->fragmented && !pinfo->flags.in_error_pkt) {
        if (reported_len < tcph->th_hlen) {
            proto_tree_add_expert_format(tcp_tree, pinfo, &ei_tcp_short_segment, tvb, offset, 0,
                                     "Short segment. Segment/fragment does not contain a full TCP header"
                                     " (might be NMAP or someone else deliberately sending unusual packets)");
            tcph->th_have_seglen = FALSE;
        } else {
            proto_item *pi;

            /* Compute the length of data in this segment. */
            tcph->th_seglen = reported_len - tcph->th_hlen;
            tcph->th_have_seglen = TRUE;

            pi = proto_tree_add_uint(ti, hf_tcp_len, tvb, offset+12, 1, tcph->th_seglen);
            PROTO_ITEM_SET_GENERATED(pi);

            /* handle TCP seq# analysis parse all new segments we see */
            if(tcp_analyze_seq) {
                if(!(pinfo->fd->visited)) {
                    tcp_analyze_sequence_number(pinfo, tcph->th_seq, tcph->th_ack, tcph->th_seglen, tcph->th_flags, tcph->th_win, tcpd);
                }
                if(tcpd && tcp_relative_seq) {
                    (tcph->th_seq) -= tcpd->fwd->base_seq;
                    if (tcph->th_flags & TH_ACK) {
                        (tcph->th_ack) -= tcpd->rev->base_seq;
                    }
                }
            }

            /* re-calculate window size, based on scaling factor */
            if (!(tcph->th_flags&TH_SYN)) {   /* SYNs are never scaled */
                if (tcpd && (tcpd->fwd->win_scale>=0)) {
                    (tcph->th_win)<<=tcpd->fwd->win_scale;
                }
                else {
                    /* Don't have it stored, so use preference setting instead! */
                    if (tcp_default_window_scaling>=0) {
                        (tcph->th_win)<<=tcp_default_window_scaling;
                    }
                }
            }

            /* Compute the sequence number of next octet after this segment. */
            nxtseq = tcph->th_seq + tcph->th_seglen;
            if ((tcph->th_flags&(TH_SYN|TH_FIN)) && (tcph->th_seglen > 0)) {
                nxtseq += 1;
            }
        }
    } else
        tcph->th_have_seglen = FALSE;

    flags_str = tcp_flags_to_str(wmem_packet_scope(), tcph);
    flags_str_first_letter = tcp_flags_to_str_first_letter(tcph);

    col_append_lstr(pinfo->cinfo, COL_INFO,
        " [", flags_str, "]",
        COL_ADD_LSTR_TERMINATOR);
    tcp_info_append_uint(pinfo, "Seq", tcph->th_seq);
    if (tcph->th_flags&TH_ACK)
        tcp_info_append_uint(pinfo, "Ack", tcph->th_ack);

    tcp_info_append_uint(pinfo, "Win", tcph->th_win);

    if (tcp_summary_in_tree) {
        proto_item_append_text(ti, ", Seq: %u", tcph->th_seq);
    }

    if (!icmp_ip) {
        if(tcp_relative_seq && tcp_analyze_seq) {
            proto_tree_add_uint_format_value(tcp_tree, hf_tcp_seq, tvb, offset + 4, 4, tcph->th_seq, "%u    (relative sequence number)", tcph->th_seq);
        } else {
            proto_tree_add_uint(tcp_tree, hf_tcp_seq, tvb, offset + 4, 4, tcph->th_seq);
        }
    }

    if (tcph->th_hlen < TCPH_MIN_LEN) {
        /* Give up at this point; we put the source and destination port in
           the tree, before fetching the header length, so that they'll
           show up if this is in the failing packet in an ICMP error packet,
           but it's now time to give up if the header length is bogus. */
        col_append_fstr(pinfo->cinfo, COL_INFO, ", bogus TCP header length (%u, must be at least %u)",
                        tcph->th_hlen, TCPH_MIN_LEN);
        if (tree) {
            tf = proto_tree_add_uint_bits_format_value(tcp_tree, hf_tcp_hdr_len, tvb, (offset + 12) << 3, 4, tcph->th_hlen,
                                                       "%u bytes (%u)", tcph->th_hlen, tcph->th_hlen >> 2);
            expert_add_info_format(pinfo, tf, &ei_tcp_bogus_header_length,
                                   "Bogus TCP header length (%u, must be at least %u)", tcph->th_hlen, TCPH_MIN_LEN);
        }
        return offset+12;
    }

    if (tcp_summary_in_tree) {
        if(tcph->th_flags&TH_ACK) {
            proto_item_append_text(ti, ", Ack: %u", tcph->th_ack);
        }
        if (tcph->th_have_seglen)
            proto_item_append_text(ti, ", Len: %u", tcph->th_seglen);
    }
    proto_item_set_len(ti, tcph->th_hlen);
    if (tcph->th_have_seglen) {
        if(tcp_relative_seq && tcp_analyze_seq) {
            tf=proto_tree_add_uint_format_value(tcp_tree, hf_tcp_nxtseq, tvb, offset, 0, nxtseq, "%u    (relative sequence number)", nxtseq);
        } else {
            tf=proto_tree_add_uint(tcp_tree, hf_tcp_nxtseq, tvb, offset, 0, nxtseq);
        }
        PROTO_ITEM_SET_GENERATED(tf);
    }

    tf = proto_tree_add_uint(tcp_tree, hf_tcp_ack, tvb, offset + 8, 4, tcph->th_ack);
    if (tcph->th_flags & TH_ACK) {
        if (tcp_relative_seq && tcp_analyze_seq) {
            proto_item_append_text(tf, "    (relative ack number)");
        }
    } else {
        /* Note if the ACK field is non-zero */
        if (tvb_get_ntohl(tvb, offset+8) != 0) {
            expert_add_info(pinfo, tf, &ei_tcp_ack_nonzero);
        }
    }

    if (tree) {
        // This should be consistent with ip.hdr_len.
        proto_tree_add_uint_bits_format_value(tcp_tree, hf_tcp_hdr_len, tvb, (offset + 12) << 3, 4, tcph->th_hlen,
            "%u bytes (%u)", tcph->th_hlen, tcph->th_hlen>>2);
        tf = proto_tree_add_uint_format(tcp_tree, hf_tcp_flags, tvb, offset + 12, 2,
                                        tcph->th_flags, "Flags: 0x%03x (%s)", tcph->th_flags, flags_str);
        field_tree = proto_item_add_subtree(tf, ett_tcp_flags);
        proto_tree_add_boolean(field_tree, hf_tcp_flags_res, tvb, offset + 12, 1, tcph->th_flags);
        proto_tree_add_boolean(field_tree, hf_tcp_flags_ns, tvb, offset + 12, 1, tcph->th_flags);
        proto_tree_add_boolean(field_tree, hf_tcp_flags_cwr, tvb, offset + 13, 1, tcph->th_flags);
        proto_tree_add_boolean(field_tree, hf_tcp_flags_ecn, tvb, offset + 13, 1, tcph->th_flags);
        proto_tree_add_boolean(field_tree, hf_tcp_flags_urg, tvb, offset + 13, 1, tcph->th_flags);
        proto_tree_add_boolean(field_tree, hf_tcp_flags_ack, tvb, offset + 13, 1, tcph->th_flags);
        proto_tree_add_boolean(field_tree, hf_tcp_flags_push, tvb, offset + 13, 1, tcph->th_flags);
        tf_rst = proto_tree_add_boolean(field_tree, hf_tcp_flags_reset, tvb, offset + 13, 1, tcph->th_flags);
        tf_syn = proto_tree_add_boolean(field_tree, hf_tcp_flags_syn, tvb, offset + 13, 1, tcph->th_flags);
        tf_fin = proto_tree_add_boolean(field_tree, hf_tcp_flags_fin, tvb, offset + 13, 1, tcph->th_flags);

        tf = proto_tree_add_string(field_tree, hf_tcp_flags_str, tvb, offset + 12, 2, flags_str_first_letter);
        PROTO_ITEM_SET_GENERATED(tf);
        /* As discussed in bug 5541, it is better to use two separate
         * fields for the real and calculated window size.
         */
        proto_tree_add_uint(tcp_tree, hf_tcp_window_size_value, tvb, offset + 14, 2, real_window);
        scaled_pi = proto_tree_add_uint(tcp_tree, hf_tcp_window_size, tvb, offset + 14, 2, tcph->th_win);
        PROTO_ITEM_SET_GENERATED(scaled_pi);

        if( !(tcph->th_flags&TH_SYN) && tcpd ) {
            switch (tcpd->fwd->win_scale) {

            case -1:
                {
                    gint16 win_scale = tcpd->fwd->win_scale;
                    gboolean override_with_pref = FALSE;

                    /* Use preference setting (if set) */
                    if (tcp_default_window_scaling != WindowScaling_NotKnown) {
                        win_scale = tcp_default_window_scaling;
                        override_with_pref = TRUE;
                    }

                    scaled_pi = proto_tree_add_int_format_value(tcp_tree, hf_tcp_window_size_scalefactor, tvb, offset + 14, 2,
                                                          win_scale, "%d (%s)",
                                                          win_scale,
                                                          (override_with_pref) ? "missing - taken from preference" : "unknown");
                    PROTO_ITEM_SET_GENERATED(scaled_pi);
                }
                break;

            case -2:
                scaled_pi = proto_tree_add_int_format_value(tcp_tree, hf_tcp_window_size_scalefactor, tvb, offset + 14, 2, tcpd->fwd->win_scale, "%d (no window scaling used)", tcpd->fwd->win_scale);
                PROTO_ITEM_SET_GENERATED(scaled_pi);
                break;

            default:
                scaled_pi = proto_tree_add_int_format_value(tcp_tree, hf_tcp_window_size_scalefactor, tvb, offset + 14, 2, 1<<tcpd->fwd->win_scale, "%d", 1<<tcpd->fwd->win_scale);
                PROTO_ITEM_SET_GENERATED(scaled_pi);
            }
        }
    }

    if(tcph->th_flags & TH_SYN) {
        if(tcph->th_flags & TH_ACK) {
           expert_add_info_format(pinfo, tf_syn, &ei_tcp_connection_sack,
                                  "Connection establish acknowledge (SYN+ACK): server port %u", tcph->th_sport);
           /* Save the server port to help determine dissector used */
           tcpd->server_port = tcph->th_sport;
        }
        else {
           expert_add_info_format(pinfo, tf_syn, &ei_tcp_connection_syn,
                                  "Connection establish request (SYN): server port %u", tcph->th_dport);
           /* Save the server port to help determine dissector used */
           tcpd->server_port = tcph->th_dport;
           tcpd->ts_mru_syn = pinfo->abs_ts;
        }
        /* Remember where the next segment will start. */
        if (tcp_desegment && tcp_reassemble_out_of_order && tcpd && !PINFO_FD_VISITED(pinfo)) {
            if (tcpd->fwd->maxnextseq == 0) {
                tcpd->fwd->maxnextseq = tcph->th_seq + 1;
            }
        }
    }
    if(tcph->th_flags & TH_FIN) {
        /* XXX - find a way to know the server port and output only that one */
        expert_add_info(pinfo, tf_fin, &ei_tcp_connection_fin);
    }
    if(tcph->th_flags & TH_RST)
        /* XXX - find a way to know the server port and output only that one */
        expert_add_info(pinfo, tf_rst, &ei_tcp_connection_rst);

    if(tcp_analyze_seq
            && (tcph->th_flags & (TH_SYN|TH_ACK)) == TH_ACK
            && !nstime_is_zero(&tcpd->ts_mru_syn)
            &&  nstime_is_zero(&tcpd->ts_first_rtt)) {
        /* If all of the following:
         * - we care (the pref is set)
         * - this is a pure ACK
         * - we have a timestamp for the most-recently-transmitted SYN
         * - we haven't seen a pure ACK yet (no ts_first_rtt stored)
         * then assume it's the last part of the handshake and store the initial
         * RTT time
         */
        nstime_delta(&(tcpd->ts_first_rtt), &(pinfo->abs_ts), &(tcpd->ts_mru_syn));
    }

    /* Supply the sequence number of the first byte and of the first byte
       after the segment. */
    tcpinfo.seq = tcph->th_seq;
    tcpinfo.nxtseq = nxtseq;
    tcpinfo.lastackseq = tcph->th_ack;

    /* Assume we'll pass un-reassembled data to subdissectors. */
    tcpinfo.is_reassembled = FALSE;

    /*
     * Assume, initially, that we can't desegment.
     */
    pinfo->can_desegment = 0;
    th_sum = tvb_get_ntohs(tvb, offset + 16);
    if (!pinfo->fragmented && tvb_bytes_exist(tvb, 0, reported_len)) {
        /* The packet isn't part of an un-reassembled fragmented datagram
           and isn't truncated.  This means we have all the data, and thus
           can checksum it and, unless it's being returned in an error
           packet, are willing to allow subdissectors to request reassembly
           on it. */

        if (tcp_check_checksum) {
            /* We haven't turned checksum checking off; checksum it. */

            /* Set up the fields of the pseudo-header. */
            SET_CKSUM_VEC_PTR(cksum_vec[0], (const guint8 *)pinfo->src.data, pinfo->src.len);
            SET_CKSUM_VEC_PTR(cksum_vec[1], (const guint8 *)pinfo->dst.data, pinfo->dst.len);
            switch (pinfo->src.type) {

            case AT_IPv4:
                phdr[0] = g_htonl((IP_PROTO_TCP<<16) + reported_len);
                SET_CKSUM_VEC_PTR(cksum_vec[2], (const guint8 *)phdr, 4);
                break;

            case AT_IPv6:
                phdr[0] = g_htonl(reported_len);
                phdr[1] = g_htonl(IP_PROTO_TCP);
                SET_CKSUM_VEC_PTR(cksum_vec[2], (const guint8 *)phdr, 8);
                break;

            default:
                /* TCP runs only atop IPv4 and IPv6.... */
                DISSECTOR_ASSERT_NOT_REACHED();
                break;
            }
            SET_CKSUM_VEC_TVB(cksum_vec[3], tvb, offset, reported_len);
            computed_cksum = in_cksum(cksum_vec, 4);
            if (computed_cksum == 0 && th_sum == 0xffff) {
                item = proto_tree_add_uint_format_value(tcp_tree, hf_tcp_checksum, tvb,
                                                  offset + 16, 2, th_sum,
                                                  "0x%04x [should be 0x0000 (see RFC 1624)]", th_sum);

                checksum_tree = proto_item_add_subtree(item, ett_tcp_checksum);
                item = proto_tree_add_uint(checksum_tree, hf_tcp_checksum_calculated, tvb,
                                              offset + 16, 2, 0x0000);
                PROTO_ITEM_SET_GENERATED(item);
                /* XXX - What should this special status be? */
                item = proto_tree_add_uint(checksum_tree, hf_tcp_checksum_status, tvb,
                                              offset + 16, 0, 4);
                PROTO_ITEM_SET_GENERATED(item);
                expert_add_info(pinfo, item, &ei_tcp_checksum_ffff);

                col_append_str(pinfo->cinfo, COL_INFO, " [TCP CHECKSUM 0xFFFF]");

                /* Checksum is treated as valid on most systems, so we're willing to desegment it. */
                desegment_ok = TRUE;
            } else {
                proto_item* calc_item;
                item = proto_tree_add_checksum(tcp_tree, tvb, offset+16, hf_tcp_checksum, hf_tcp_checksum_status, &ei_tcp_checksum_bad, pinfo, computed_cksum,
                                               ENC_BIG_ENDIAN, PROTO_CHECKSUM_VERIFY|PROTO_CHECKSUM_IN_CKSUM);

                calc_item = proto_tree_add_uint(tcp_tree, hf_tcp_checksum_calculated, tvb,
                                              offset + 16, 2, in_cksum_shouldbe(th_sum, computed_cksum));
                PROTO_ITEM_SET_GENERATED(calc_item);

                /* Checksum is valid, so we're willing to desegment it. */
                if (computed_cksum == 0) {
                    desegment_ok = TRUE;
                } else {
                    proto_item_append_text(item, "(maybe caused by \"TCP checksum offload\"?)");

                    /* Checksum is invalid, so we're not willing to desegment it. */
                    desegment_ok = FALSE;
                    pinfo->noreassembly_reason = " [incorrect TCP checksum]";
                    col_append_str(pinfo->cinfo, COL_INFO, " [TCP CHECKSUM INCORRECT]");
                }
            }
        } else {
            proto_tree_add_checksum(tcp_tree, tvb, offset+16, hf_tcp_checksum, hf_tcp_checksum_status, &ei_tcp_checksum_bad, pinfo, 0,
                                    ENC_BIG_ENDIAN, PROTO_CHECKSUM_NO_FLAGS);

            /* We didn't check the checksum, and don't care if it's valid,
               so we're willing to desegment it. */
            desegment_ok = TRUE;
        }
    } else {
        /* We don't have all the packet data, so we can't checksum it... */
        proto_tree_add_checksum(tcp_tree, tvb, offset+16, hf_tcp_checksum, hf_tcp_checksum_status, &ei_tcp_checksum_bad, pinfo, 0,
                                    ENC_BIG_ENDIAN, PROTO_CHECKSUM_NO_FLAGS);

        /* ...and aren't willing to desegment it. */
        desegment_ok = FALSE;
    }

    if (desegment_ok) {
        /* We're willing to desegment this.  Is desegmentation enabled? */
        if (tcp_desegment) {
            /* Yes - is this segment being returned in an error packet? */
            if (!pinfo->flags.in_error_pkt) {
                /* No - indicate that we will desegment.
                   We do NOT want to desegment segments returned in error
                   packets, as they're not part of a TCP connection. */
                pinfo->can_desegment = 2;
            }
        }
    }

    item = proto_tree_add_item_ret_uint(tcp_tree, hf_tcp_urgent_pointer, tvb, offset + 18, 2, ENC_BIG_ENDIAN, &th_urp);

    if (IS_TH_URG(tcph->th_flags)) {
        /* Export the urgent pointer, for the benefit of protocols such as
           rlogin. */
        tcpinfo.urgent_pointer = (guint16)th_urp;
        tcp_info_append_uint(pinfo, "Urg", th_urp);
    } else {
         if (th_urp) {
            /* Note if the urgent pointer field is non-zero */
            expert_add_info(pinfo, item, &ei_tcp_urgent_pointer_non_zero);
         }
    }

    if (tcph->th_have_seglen)
        tcp_info_append_uint(pinfo, "Len", tcph->th_seglen);

    /* If there's more than just the fixed-length header (20 bytes), create
       a protocol tree item for the options.  (We already know there's
       not less than the fixed-length header - we checked that above.)

       We ensure that we don't throw an exception here, so that we can
       do some analysis before we dissect the options and possibly
       throw an exception.  (Trying to avoid throwing an exception when
       dissecting options is not something we should do.) */
    optlen = tcph->th_hlen - TCPH_MIN_LEN; /* length of options, in bytes */
    options_item = NULL;
    options_tree = NULL;
    if (optlen != 0) {
        guint bc = (guint)tvb_captured_length_remaining(tvb, offset + 20);

        if (tcp_tree != NULL) {
            options_item = proto_tree_add_item(tcp_tree, hf_tcp_options, tvb, offset + 20,
                                               bc < optlen ? bc : optlen, ENC_NA);
            proto_item_set_text(options_item, "Options: (%u bytes)", optlen);
            options_tree = proto_item_add_subtree(options_item, ett_tcp_options);
        }
    }

    tcph->num_sack_ranges = 0;

    /* handle TCP seq# analysis, print any extra SEQ/ACK data for this segment*/
    if(tcp_analyze_seq) {
        guint32 use_seq = tcph->th_seq;
        guint32 use_ack = tcph->th_ack;
        /* May need to recover absolute values here... */
        if (tcp_relative_seq) {
            use_seq += tcpd->fwd->base_seq;
            if (tcph->th_flags & TH_ACK) {
                use_ack += tcpd->rev->base_seq;
            }
        }
        tcp_print_sequence_number_analysis(pinfo, tvb, tcp_tree, tcpd, use_seq, use_ack);
    }

    /* handle conversation timestamps */
    if(tcp_calculate_ts) {
        tcp_print_timestamps(pinfo, tvb, tcp_tree, tcpd, tcppd);
    }

    /* Now dissect the options. */
    if (optlen) {
        rvbd_option_data* option_data;

        tcp_dissect_options(tvb, offset + 20, optlen,
                               TCPOPT_EOL, pinfo, options_tree,
                               options_item, tcph);

        /* Do some post evaluation of some Riverbed probe options in the list */
        option_data = (rvbd_option_data*)p_get_proto_data(pinfo->pool, pinfo, proto_tcp_option_rvbd_probe, pinfo->curr_layer_num);
        if (option_data != NULL)
        {
            if (option_data->valid)
            {
                /* Distinguish S+ from S+* */
                col_prepend_fstr(pinfo->cinfo, COL_INFO, "S%s, ",
                                     option_data->type == PROBE_TRACE ? "#" :
                                     (option_data->probe_flags & RVBD_FLAGS_PROBE_NCFE) ? "+*" : "+");
            }
        }

    }

    if(!pinfo->fd->visited) {
        if((tcph->th_flags & TH_SYN)==TH_SYN) {
            /* Check the validity of the window scale value
             */
            verify_tcp_window_scaling((tcph->th_flags&TH_ACK)==TH_ACK,tcpd);
        }

        if((tcph->th_flags & (TH_SYN|TH_ACK))==(TH_SYN|TH_ACK)) {
            /* If the SYN or the SYN+ACK offered SCPS capabilities,
             * validate the flow's bidirectional scps capabilities.
             * The or protects against broken implementations offering
             * SCPS capabilities on SYN+ACK even if it wasn't offered with the SYN
             */
            if(tcpd && ((tcpd->rev->scps_capable) || (tcpd->fwd->scps_capable))) {
                verify_scps(pinfo, tf_syn, tcpd);
            }

        }
    }

    if (tcph->th_mptcp) {

        if (tcp_analyze_mptcp) {
            mptcp_add_analysis_subtree(pinfo, tvb, tcp_tree, tcpd, tcpd->mptcp_analysis, tcph );
        }
    }

    /* Skip over header + options */
    offset += tcph->th_hlen;

    /* Check the packet length to see if there's more data
       (it could be an ACK-only packet) */
    captured_length_remaining = tvb_captured_length_remaining(tvb, offset);

    if (tcph->th_have_seglen) {
        if(have_tap_listener(tcp_follow_tap)) {
            tcp_follow_tap_data_t* follow_data = wmem_new0(wmem_packet_scope(), tcp_follow_tap_data_t);

            follow_data->tvb = tvb_new_subset_remaining(tvb, offset);
            follow_data->tcph = tcph;
            follow_data->tcpd = tcpd;

            tap_queue_packet(tcp_follow_tap, pinfo, follow_data);
        }
    }

    tap_queue_packet(tcp_tap, pinfo, tcph);

    /* if it is an MPTCP packet */
    if(tcpd->mptcp_analysis) {
        tap_queue_packet(mptcp_tap, pinfo, tcpd);
    }

    /* If we're reassembling something whose length isn't known
     * beforehand, and that runs all the way to the end of
     * the data stream, a FIN indicates the end of the data
     * stream and thus the completion of reassembly, so we
     * need to explicitly check for that here.
     */
    if(tcph->th_have_seglen && tcpd && (tcph->th_flags & TH_FIN)
       && (tcpd->fwd->flags&TCP_FLOW_REASSEMBLE_UNTIL_FIN) ) {
        struct tcp_multisegment_pdu *msp;

        /* Is this the FIN that ended the data stream or is it a
         * retransmission of that FIN?
         */
        if (tcpd->fwd->fin == 0 || tcpd->fwd->fin == pinfo->num) {
            /* Either we haven't seen a FIN for this flow or we
             * have and it's this frame. Note that this is the FIN
             * for this flow, terminate reassembly and dissect the
             * results. */
            tcpd->fwd->fin = pinfo->num;
            msp=(struct tcp_multisegment_pdu *)wmem_tree_lookup32_le(tcpd->fwd->multisegment_pdus, tcph->th_seq-1);
            if(msp) {
                fragment_head *ipfd_head;

                ipfd_head = fragment_add(&tcp_reassembly_table, tvb, offset,
                                         pinfo, msp->first_frame, NULL,
                                         tcph->th_seq - msp->seq,
                                         tcph->th_seglen,
                                         FALSE );
                if(ipfd_head) {
                    tvbuff_t *next_tvb;

                    /* create a new TVB structure for desegmented data
                     * datalen-1 to strip the dummy FIN byte off
                     */
                    next_tvb = tvb_new_chain(tvb, ipfd_head->tvb_data);

                    /* add desegmented data to the data source list */
                    add_new_data_source(pinfo, next_tvb, "Reassembled TCP");

                    /* Show details of the reassembly */
                    print_tcp_fragment_tree(ipfd_head, tree, tcp_tree, pinfo, next_tvb);

                    /* call the payload dissector
                     * but make sure we don't offer desegmentation any more
                     */
                    pinfo->can_desegment = 0;

                    process_tcp_payload(next_tvb, 0, pinfo, tree, tcp_tree, tcph->th_sport, tcph->th_dport, tcph->th_seq,
                                        nxtseq, FALSE, tcpd, &tcpinfo);

                    return tvb_captured_length(tvb);
                }
            }
        } else {
            /* Yes.  This is a retransmission of the final FIN (or it's
             * the final FIN transmitted via a different path).
             * XXX - we need to flag retransmissions a bit better.
             */
            proto_tree_add_uint(tcp_tree, hf_tcp_fin_retransmission, tvb, 0, 0, tcpd->fwd->fin);
        }
    }

    if (tcp_display_process_info && tcpd && ((tcpd->fwd && tcpd->fwd->process_info && tcpd->fwd->process_info->command) ||
                 (tcpd->rev && tcpd->rev->process_info && tcpd->rev->process_info->command))) {
        field_tree = proto_tree_add_subtree(tcp_tree, tvb, offset, 0, ett_tcp_process_info, &ti, "Process Information");
        PROTO_ITEM_SET_GENERATED(ti);
        if (tcpd->fwd && tcpd->fwd->process_info && tcpd->fwd->process_info->command) {
            proto_tree_add_uint(field_tree, hf_tcp_proc_dst_uid, tvb, 0, 0, tcpd->fwd->process_info->process_uid);
            proto_tree_add_uint(field_tree, hf_tcp_proc_dst_pid, tvb, 0, 0, tcpd->fwd->process_info->process_pid);
            proto_tree_add_string(field_tree, hf_tcp_proc_dst_uname, tvb, 0, 0, tcpd->fwd->process_info->username);
            proto_tree_add_string(field_tree, hf_tcp_proc_dst_cmd, tvb, 0, 0, tcpd->fwd->process_info->command);
        }
        if (tcpd->rev && tcpd->rev->process_info && tcpd->rev->process_info->command) {
            proto_tree_add_uint(field_tree, hf_tcp_proc_src_uid, tvb, 0, 0, tcpd->rev->process_info->process_uid);
            proto_tree_add_uint(field_tree, hf_tcp_proc_src_pid, tvb, 0, 0, tcpd->rev->process_info->process_pid);
            proto_tree_add_string(field_tree, hf_tcp_proc_src_uname, tvb, 0, 0, tcpd->rev->process_info->username);
            proto_tree_add_string(field_tree, hf_tcp_proc_src_cmd, tvb, 0, 0, tcpd->rev->process_info->command);
        }
    }

    /*
     * XXX - what, if any, of this should we do if this is included in an
     * error packet?  It might be nice to see the details of the packet
     * that caused the ICMP error, but it might not be nice to have the
     * dissector update state based on it.
     * Also, we probably don't want to run TCP taps on those packets.
     */
    if (captured_length_remaining != 0) {
        if (tcph->th_flags & TH_RST) {
            /*
             * RFC1122 says:
             *
             *  4.2.2.12  RST Segment: RFC-793 Section 3.4
             *
             *    A TCP SHOULD allow a received RST segment to include data.
             *
             *    DISCUSSION
             *         It has been suggested that a RST segment could contain
             *         ASCII text that encoded and explained the cause of the
             *         RST.  No standard has yet been established for such
             *         data.
             *
             * so for segments with RST we just display the data as text.
             */
            proto_tree_add_item(tcp_tree, hf_tcp_reset_cause, tvb, offset, captured_length_remaining, ENC_NA|ENC_ASCII);
        } else {
            /*
             * XXX - dissect_tcp_payload() expects the payload length, however
             * SYN and FIN increments the nxtseq by one without having
             * the data.
             */
            if ((tcph->th_flags&(TH_FIN|TH_SYN)) && (tcph->th_seglen > 0)) {
                nxtseq -= 1;
            }
            dissect_tcp_payload(tvb, pinfo, offset, tcph->th_seq, nxtseq,
                                tcph->th_sport, tcph->th_dport, tree, tcp_tree, tcpd, &tcpinfo);
        }
    }
    return tvb_captured_length(tvb);
}