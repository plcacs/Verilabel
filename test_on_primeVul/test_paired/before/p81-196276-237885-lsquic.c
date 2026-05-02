lsquic_qeh_settings (struct qpack_enc_hdl *qeh, unsigned max_table_size,
             unsigned dyn_table_size, unsigned max_risked_streams, int server)
{
    enum lsqpack_enc_opts enc_opts;

    assert(qeh->qeh_flags & QEH_INITIALIZED);

    if (qeh->qeh_flags & QEH_HAVE_SETTINGS)
    {
        LSQ_WARN("settings already set");
        return -1;
    }

    enc_opts = LSQPACK_ENC_OPT_STAGE_2
             | (server ? LSQPACK_ENC_OPT_SERVER : 0);
    qeh->qeh_tsu_sz = sizeof(qeh->qeh_tsu_buf);
    if (0 != lsqpack_enc_init(&qeh->qeh_encoder, (void *) qeh->qeh_conn,
                max_table_size, dyn_table_size, max_risked_streams, enc_opts,
                qeh->qeh_tsu_buf, &qeh->qeh_tsu_sz))
    {
        LSQ_INFO("could not initialize QPACK encoder");
        return -1;
    }
    LSQ_DEBUG("%zu-byte post-init TSU", qeh->qeh_tsu_sz);
    qeh->qeh_flags |= QEH_HAVE_SETTINGS;
    qeh->qeh_max_prefix_size =
                        lsqpack_enc_header_block_prefix_size(&qeh->qeh_encoder);
    LSQ_DEBUG("have settings: max table size=%u; dyn table size=%u; max risked "
        "streams=%u", max_table_size, dyn_table_size, max_risked_streams);
    if (qeh->qeh_enc_sm_out)
        qeh_begin_out(qeh);
    return 0;
}