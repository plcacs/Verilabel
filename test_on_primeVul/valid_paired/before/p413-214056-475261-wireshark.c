static int dissect_dvb_s2_bb(tvbuff_t *tvb, int cur_off, proto_tree *tree, packet_info *pinfo)
{
    proto_item *ti;
    proto_tree *dvb_s2_bb_tree;

    guint8      input8, matype1;
    guint8      sync_flag = 0;
    guint16     input16, bb_data_len = 0, user_packet_length;

    int         sub_dissected        = 0, flag_is_ms = 0, new_off = 0;

    static int * const bb_header_bitfields[] = {
        &hf_dvb_s2_bb_matype1_gs,
        &hf_dvb_s2_bb_matype1_mis,
        &hf_dvb_s2_bb_matype1_acm,
        &hf_dvb_s2_bb_matype1_issyi,
        &hf_dvb_s2_bb_matype1_npd,
        &hf_dvb_s2_bb_matype1_low_ro,
        NULL
    };

    col_append_str(pinfo->cinfo, COL_PROTOCOL, "BB ");
    col_append_str(pinfo->cinfo, COL_INFO, "Baseband ");

    /* create display subtree for the protocol */
    ti = proto_tree_add_item(tree, proto_dvb_s2_bb, tvb, cur_off, DVB_S2_BB_HEADER_LEN, ENC_NA);
    dvb_s2_bb_tree = proto_item_add_subtree(ti, ett_dvb_s2_bb);

    matype1 = tvb_get_guint8(tvb, cur_off + DVB_S2_BB_OFFS_MATYPE1);
    new_off += 1;

    if (BIT_IS_CLEAR(matype1, DVB_S2_BB_MIS_POS))
        flag_is_ms = 1;

    proto_tree_add_bitmask_with_flags(dvb_s2_bb_tree, tvb, cur_off + DVB_S2_BB_OFFS_MATYPE1, hf_dvb_s2_bb_matype1,
        ett_dvb_s2_bb_matype1, bb_header_bitfields, ENC_BIG_ENDIAN, BMT_NO_FLAGS);

    input8 = tvb_get_guint8(tvb, cur_off + DVB_S2_BB_OFFS_MATYPE1);

    if ((pinfo->fd->num == 1) && (_use_low_rolloff_value != 0)) {
        _use_low_rolloff_value = 0;
    }
    if (((input8 & 0x03) == 3) && !_use_low_rolloff_value) {
      _use_low_rolloff_value = 1;
    }
    if (_use_low_rolloff_value) {
       proto_tree_add_item(dvb_s2_bb_tree, hf_dvb_s2_bb_matype1_low_ro, tvb,
                           cur_off + DVB_S2_BB_OFFS_MATYPE1, 1, ENC_BIG_ENDIAN);
    } else {
       proto_tree_add_item(dvb_s2_bb_tree, hf_dvb_s2_bb_matype1_high_ro, tvb,
                           cur_off + DVB_S2_BB_OFFS_MATYPE1, 1, ENC_BIG_ENDIAN);
    }

    input8 = tvb_get_guint8(tvb, cur_off + DVB_S2_BB_OFFS_MATYPE2);
    new_off += 1;
    if (flag_is_ms) {
        proto_tree_add_uint_format_value(dvb_s2_bb_tree, hf_dvb_s2_bb_matype2, tvb,
                                   cur_off + DVB_S2_BB_OFFS_MATYPE2, 1, input8, "Input Stream Identifier (ISI): %d",
                                   input8);
    } else {
        proto_tree_add_uint_format_value(dvb_s2_bb_tree, hf_dvb_s2_bb_matype2, tvb,
                                   cur_off + DVB_S2_BB_OFFS_MATYPE2, 1, input8, "reserved");
    }

    user_packet_length = input16 = tvb_get_ntohs(tvb, cur_off + DVB_S2_BB_OFFS_UPL);
    new_off += 2;

    proto_tree_add_uint_format(dvb_s2_bb_tree, hf_dvb_s2_bb_upl, tvb,
                               cur_off + DVB_S2_BB_OFFS_UPL, 2, input16, "User Packet Length: %d bits (%d bytes)",
                               (guint16) input16, (guint16) input16 / 8);

    bb_data_len = input16 = tvb_get_ntohs(tvb, cur_off + DVB_S2_BB_OFFS_DFL);
    bb_data_len /= 8;
    new_off += 2;

    proto_tree_add_uint_format_value(dvb_s2_bb_tree, hf_dvb_s2_bb_dfl, tvb,
                               cur_off + DVB_S2_BB_OFFS_DFL, 2, input16, "%d bits (%d bytes)", input16, input16 / 8);

    new_off += 1;
    sync_flag = tvb_get_guint8(tvb, cur_off + DVB_S2_BB_OFFS_SYNC);
    proto_tree_add_item(dvb_s2_bb_tree, hf_dvb_s2_bb_sync, tvb, cur_off + DVB_S2_BB_OFFS_SYNC, 1, ENC_BIG_ENDIAN);

    new_off += 2;
    proto_tree_add_item(dvb_s2_bb_tree, hf_dvb_s2_bb_syncd, tvb, cur_off + DVB_S2_BB_OFFS_SYNCD, 2, ENC_BIG_ENDIAN);

    new_off += 1;
    proto_tree_add_checksum(dvb_s2_bb_tree, tvb, cur_off + DVB_S2_BB_OFFS_CRC, hf_dvb_s2_bb_crc, hf_dvb_s2_bb_crc_status, &ei_dvb_s2_bb_crc, pinfo,
        compute_crc8(tvb, DVB_S2_BB_HEADER_LEN - 1, cur_off), ENC_NA, PROTO_CHECKSUM_VERIFY);

    switch (matype1 & DVB_S2_BB_TSGS_MASK) {
    case DVB_S2_BB_TSGS_GENERIC_CONTINUOUS:
        /* Check GSE constraints on the BB header per 9.2.1 of ETSI TS 102 771 */
        if (BIT_IS_SET(matype1, DVB_S2_BB_ISSYI_POS)) {
            expert_add_info(pinfo, ti, &ei_dvb_s2_bb_issy_invalid);
        }
        if (BIT_IS_SET(matype1, DVB_S2_BB_NPD_POS)) {
            expert_add_info(pinfo, ti, &ei_dvb_s2_bb_npd_invalid);
        }
        if (user_packet_length != 0x0000) {
            expert_add_info_format(pinfo, ti, &ei_dvb_s2_bb_upl_invalid,
                "UPL is 0x%04x. It must be 0x0000 for GSE packets.", user_packet_length);
        }


        if (dvb_s2_df_dissection) {
            while (bb_data_len) {
                if (sync_flag == DVB_S2_BB_SYNC_EIP_CRC32 && bb_data_len == DVB_S2_BB_EIP_CRC32_LEN) {
                    proto_tree_add_item(dvb_s2_bb_tree, hf_dvb_s2_bb_eip_crc32, tvb, cur_off + new_off, bb_data_len, ENC_NA);
                    bb_data_len = 0;
                    new_off += DVB_S2_BB_EIP_CRC32_LEN;
                } else {
                    /* start DVB-GSE dissector */
                    sub_dissected = dissect_dvb_s2_gse(tvb, cur_off + new_off, tree, pinfo, bb_data_len);
                    new_off += sub_dissected;

                    if ((sub_dissected <= bb_data_len) && (sub_dissected >= DVB_S2_GSE_MINSIZE)) {
                        bb_data_len -= sub_dissected;
                        if (bb_data_len < DVB_S2_GSE_MINSIZE)
                            bb_data_len = 0;
                    }
                }
            }
        } else {
            proto_tree_add_item(dvb_s2_bb_tree, hf_dvb_s2_bb_df, tvb, cur_off + new_off, bb_data_len, ENC_NA);
            new_off += bb_data_len;
        }
        break;

    case DVB_S2_BB_TSGS_GENERIC_PACKETIZED:
        proto_tree_add_item(tree, hf_dvb_s2_bb_packetized, tvb, cur_off + new_off, bb_data_len, ENC_NA);
        new_off += bb_data_len;
        break;

    case DVB_S2_BB_TSGS_TRANSPORT_STREAM:
        proto_tree_add_item(tree, hf_dvb_s2_bb_transport, tvb, cur_off + new_off, bb_data_len, ENC_NA);
        new_off += bb_data_len;
        break;

    default:
        proto_tree_add_item(tree, hf_dvb_s2_bb_reserved, tvb, cur_off + new_off,bb_data_len, ENC_NA);
        new_off += bb_data_len;
        expert_add_info(pinfo, ti, &ei_dvb_s2_bb_reserved);
        break;
    }

    return new_off;
}