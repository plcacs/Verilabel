dissect_dnp3_message(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, void* data _U_)
{
  proto_item  *ti, *tdl, *tc, *hidden_item;
  proto_tree  *dnp3_tree, *dl_tree, *field_tree;
  int          offset = 0, temp_offset = 0;
  gboolean     dl_prm;
  guint8       dl_len, dl_ctl, dl_func;
  const gchar *func_code_str;
  guint16      dl_dst, dl_src, calc_dl_crc;

  /* Make entries in Protocol column and Info column on summary display */
  col_set_str(pinfo->cinfo, COL_PROTOCOL, "DNP 3.0");
  col_clear(pinfo->cinfo, COL_INFO);

  /* Skip "0x0564" header bytes */
  temp_offset += 2;

  dl_len = tvb_get_guint8(tvb, temp_offset);
  temp_offset += 1;

  dl_ctl = tvb_get_guint8(tvb, temp_offset);
  temp_offset += 1;

  dl_dst = tvb_get_letohs(tvb, temp_offset);
  temp_offset += 2;

  dl_src = tvb_get_letohs(tvb, temp_offset);

  dl_func = dl_ctl & DNP3_CTL_FUNC;
  dl_prm = dl_ctl & DNP3_CTL_PRM;
  func_code_str = val_to_str(dl_func, dl_prm ? dnp3_ctl_func_pri_vals : dnp3_ctl_func_sec_vals,
           "Unknown function (0x%02x)");

  /* Make sure source and dest are always in the info column */
  col_append_fstr(pinfo->cinfo, COL_INFO, "from %u to %u", dl_src, dl_dst);
  col_append_sep_fstr(pinfo->cinfo, COL_INFO, NULL, "len=%u, %s", dl_len, func_code_str);

  /* create display subtree for the protocol */
  ti = proto_tree_add_item(tree, proto_dnp3, tvb, offset, -1, ENC_NA);
  dnp3_tree = proto_item_add_subtree(ti, ett_dnp3);

  /* Create Subtree for Data Link Layer */
  dl_tree = proto_tree_add_subtree_format(dnp3_tree, tvb, offset, DNP_HDR_LEN, ett_dnp3_dl, &tdl,
        "Data Link Layer, Len: %u, From: %u, To: %u, ", dl_len, dl_src, dl_dst);
  if (dl_prm) {
    if (dl_ctl & DNP3_CTL_DIR) proto_item_append_text(tdl, "DIR, ");
    if (dl_ctl & DNP3_CTL_PRM) proto_item_append_text(tdl, "PRM, ");
    if (dl_ctl & DNP3_CTL_FCB) proto_item_append_text(tdl, "FCB, ");
    if (dl_ctl & DNP3_CTL_FCV) proto_item_append_text(tdl, "FCV, ");
  }
  else {
    if (dl_ctl & DNP3_CTL_DIR) proto_item_append_text(tdl, "DIR, ");
    if (dl_ctl & DNP3_CTL_PRM) proto_item_append_text(tdl, "PRM, ");
    if (dl_ctl & DNP3_CTL_RES) proto_item_append_text(tdl, "RES, ");
    if (dl_ctl & DNP3_CTL_DFC) proto_item_append_text(tdl, "DFC, ");
  }
  proto_item_append_text(tdl, "%s", func_code_str);

  /* start bytes */
  proto_tree_add_item(dl_tree, hf_dnp3_start, tvb, offset, 2, ENC_BIG_ENDIAN);
  offset += 2;

  /* add length field */
  proto_tree_add_item(dl_tree, hf_dnp3_len, tvb, offset, 1, ENC_BIG_ENDIAN);
  offset += 1;

  /* Add Control Byte Subtree */
  tc = proto_tree_add_uint_format_value(dl_tree, hf_dnp3_ctl, tvb, offset, 1, dl_ctl,
          "0x%02x (", dl_ctl);
  /* Add Text to Control Byte Subtree Header */
  if (dl_prm) {
    if (dl_ctl & DNP3_CTL_DIR) proto_item_append_text(tc, "DIR, ");
    if (dl_ctl & DNP3_CTL_PRM) proto_item_append_text(tc, "PRM, ");
    if (dl_ctl & DNP3_CTL_FCB) proto_item_append_text(tc, "FCB, ");
    if (dl_ctl & DNP3_CTL_FCV) proto_item_append_text(tc, "FCV, ");
  }
  else {
    if (dl_ctl & DNP3_CTL_DIR) proto_item_append_text(tc, "DIR, ");
    if (dl_ctl & DNP3_CTL_PRM) proto_item_append_text(tc, "PRM, ");
    if (dl_ctl & DNP3_CTL_RES) proto_item_append_text(tc, "RES, ");
    if (dl_ctl & DNP3_CTL_DFC) proto_item_append_text(tc, "DFC, ");
  }
  proto_item_append_text(tc, "%s)", func_code_str );
  field_tree = proto_item_add_subtree(tc, ett_dnp3_dl_ctl);

  /* Add Control Byte Subtree Items */
  if (dl_prm) {
    proto_tree_add_item(field_tree, hf_dnp3_ctl_dir, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(field_tree, hf_dnp3_ctl_prm, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(field_tree, hf_dnp3_ctl_fcb, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(field_tree, hf_dnp3_ctl_fcv, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(field_tree, hf_dnp3_ctl_prifunc, tvb, offset, 1, ENC_BIG_ENDIAN);
  }
  else {
    proto_tree_add_item(field_tree, hf_dnp3_ctl_dir, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(field_tree, hf_dnp3_ctl_prm, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(field_tree, hf_dnp3_ctl_dfc, tvb, offset, 1, ENC_LITTLE_ENDIAN);
    proto_tree_add_item(field_tree, hf_dnp3_ctl_secfunc, tvb, offset, 1, ENC_BIG_ENDIAN);
  }
    offset += 1;

  /* add destination and source addresses */
  proto_tree_add_item(dl_tree, hf_dnp3_dst, tvb, offset, 2, ENC_LITTLE_ENDIAN);
  hidden_item = proto_tree_add_item(dl_tree, hf_dnp3_addr, tvb, offset, 2, ENC_LITTLE_ENDIAN);
  proto_item_set_hidden(hidden_item);
  offset += 2;
  proto_tree_add_item(dl_tree, hf_dnp3_src, tvb, offset, 2, ENC_LITTLE_ENDIAN);
  hidden_item = proto_tree_add_item(dl_tree, hf_dnp3_addr, tvb, offset, 2, ENC_LITTLE_ENDIAN);
  proto_item_set_hidden(hidden_item);
  offset += 2;

  /* and header CRC */
  calc_dl_crc = calculateCRCtvb(tvb, 0, DNP_HDR_LEN - 2);
  proto_tree_add_checksum(dl_tree, tvb, offset, hf_dnp3_data_hdr_crc,
                          hf_dnp3_data_hdr_crc_status, &ei_dnp3_data_hdr_crc_incorrect,
                          pinfo, calc_dl_crc, ENC_LITTLE_ENDIAN, PROTO_CHECKSUM_VERIFY);
  offset += 2;

  /* If the DataLink function is 'Request Link Status' or 'Status of Link',
     or 'Reset Link' we don't expect any Transport or Application Layer Data
     NOTE: This code should probably check what DOES have TR or AL data */
  if ((dl_func != DL_FUNC_LINK_STAT) && (dl_func != DL_FUNC_STAT_LINK) &&
      (dl_func != DL_FUNC_RESET_LINK) && (dl_func != DL_FUNC_ACK))
  {
    proto_tree *data_tree;
    proto_item *data_ti;
    guint8      tr_ctl, tr_seq;
    gboolean    tr_fir, tr_fin;
    guint8     *al_buffer, *al_buffer_ptr;
    guint8      data_len;
    int         data_start = offset;
    int         tl_offset;
    gboolean    crc_OK = FALSE;
    tvbuff_t   *next_tvb;
    guint       i;
    static int * const transport_flags[] = {
      &hf_dnp3_tr_fin,
      &hf_dnp3_tr_fir,
      &hf_dnp3_tr_seq,
      NULL
    };

    /* get the transport layer byte */
    tr_ctl = tvb_get_guint8(tvb, offset);
    tr_seq = tr_ctl & DNP3_TR_SEQ;
    tr_fir = tr_ctl & DNP3_TR_FIR;
    tr_fin = tr_ctl & DNP3_TR_FIN;

    /* Add Transport Layer Tree */
    tc = proto_tree_add_bitmask(dnp3_tree, tvb, offset, hf_dnp3_tr_ctl, ett_dnp3_tr_ctl, transport_flags, ENC_BIG_ENDIAN);
    proto_item_append_text(tc, "(");
    if (tr_fir) proto_item_append_text(tc, "FIR, ");
    if (tr_fin) proto_item_append_text(tc, "FIN, ");
    proto_item_append_text(tc, "Sequence %u)", tr_seq);

    /* Add data chunk tree */
    data_tree = proto_tree_add_subtree(dnp3_tree, tvb, offset, -1, ett_dnp3_dl_data, &data_ti, "Data Chunks");

    /* extract the application layer data, validating the CRCs */

    /* XXX - check for dl_len <= 5 */
    data_len = dl_len - 5;
    al_buffer = (guint8 *)wmem_alloc(pinfo->pool, data_len);
    al_buffer_ptr = al_buffer;
    i = 0;
    tl_offset = 1;  /* skip the initial transport layer byte when assembling chunks for the application layer tvb */
    while (data_len > 0)
    {
      guint8        chk_size;
      const guint8 *chk_ptr;
      proto_tree   *chk_tree;
      proto_item   *chk_len_ti;
      guint16       calc_crc, act_crc;

      chk_size = MIN(data_len, AL_MAX_CHUNK_SIZE);
      chk_ptr  = tvb_get_ptr(tvb, offset, chk_size);
      memcpy(al_buffer_ptr, chk_ptr + tl_offset, chk_size - tl_offset);
      al_buffer_ptr += chk_size - tl_offset;

      chk_tree = proto_tree_add_subtree_format(data_tree, tvb, offset, chk_size + 2, ett_dnp3_dl_chunk, NULL, "Data Chunk: %u", i);
      proto_tree_add_item(chk_tree, hf_dnp3_data_chunk, tvb, offset, chk_size, ENC_NA);
      chk_len_ti = proto_tree_add_uint(chk_tree, hf_dnp3_data_chunk_len, tvb, offset, 0, chk_size);
      proto_item_set_generated(chk_len_ti);

      offset  += chk_size;

      calc_crc = calculateCRC(chk_ptr, chk_size);
      proto_tree_add_checksum(chk_tree, tvb, offset, hf_dnp3_data_chunk_crc,
                              hf_dnp3_data_chunk_crc_status, &ei_dnp3_data_chunk_crc_incorrect,
                              pinfo, calc_crc, ENC_LITTLE_ENDIAN, PROTO_CHECKSUM_VERIFY);
      act_crc  = tvb_get_letohs(tvb, offset);
      offset  += 2;
      crc_OK   = calc_crc == act_crc;
      if (!crc_OK)
      {
        /* Don't trust the rest of the data, get out of here */
        break;
      }
      data_len -= chk_size;
      i++;
      tl_offset = 0;  /* copy all the data in the rest of the chunks */
    }
    proto_item_set_len(data_ti, offset - data_start);

    /* if crc OK, set up new tvb */
    if (crc_OK)
    {
      tvbuff_t *al_tvb;
      gboolean  save_fragmented;

      al_tvb = tvb_new_child_real_data(tvb, al_buffer, (guint) (al_buffer_ptr-al_buffer), (gint) (al_buffer_ptr-al_buffer));

      /* Check for fragmented packet */
      save_fragmented = pinfo->fragmented;

      /* Reassemble AL fragments */
      static guint al_max_fragments = 60;
      static guint al_fragment_aging = 64; /* sequence numbers only 6 bit */
      fragment_head *frag_al = NULL;
      pinfo->fragmented = TRUE;
      if (!pinfo->fd->visited)
      {
        frag_al = fragment_add_seq_single_aging(&al_reassembly_table,
            al_tvb, 0, pinfo, tr_seq, NULL,
            tvb_reported_length(al_tvb), /* As this is a constructed tvb, all of it is ok */
            tr_fir, tr_fin,
            al_max_fragments, al_fragment_aging);
      }
      else
      {
        frag_al = fragment_get_reassembled_id(&al_reassembly_table, pinfo, tr_seq);
      }
      next_tvb = process_reassembled_data(al_tvb, 0, pinfo,
          "Reassembled DNP 3.0 Application Layer message", frag_al, &dnp3_frag_items,
          NULL, dnp3_tree);

      if (frag_al)
      {
        if (pinfo->num == frag_al->reassembled_in && pinfo->curr_layer_num == frag_al->reas_in_layer_num)
        {
          /* As a complete AL message will have cleared the info column,
             make sure source and dest are always in the info column */
          //col_append_fstr(pinfo->cinfo, COL_INFO, "from %u to %u", dl_src, dl_dst);
          //col_set_fence(pinfo->cinfo, COL_INFO);
          dissect_dnp3_al(next_tvb, pinfo, dnp3_tree);
        }
        else
        {
          /* Lock any column info set by the DL and TL */
          col_set_fence(pinfo->cinfo, COL_INFO);
          col_append_fstr(pinfo->cinfo, COL_INFO,
              " (Application Layer fragment %u, reassembled in packet %u)",
              tr_seq, frag_al->reassembled_in);
          proto_tree_add_item(dnp3_tree, hf_al_frag_data, al_tvb, 0, -1, ENC_NA);
        }
      }
      else
      {
        col_append_fstr(pinfo->cinfo, COL_INFO,
            " (Application Layer Unreassembled fragment %u)",
            tr_seq);
        proto_tree_add_item(dnp3_tree, hf_al_frag_data, al_tvb, 0, -1, ENC_NA);
      }

      pinfo->fragmented = save_fragmented;
    }
    else
    {
      /* CRC error - throw away the data. */
      next_tvb = NULL;
    }
  }

  /* Set the length of the message */
  proto_item_set_len(ti, offset);
  return offset;
}