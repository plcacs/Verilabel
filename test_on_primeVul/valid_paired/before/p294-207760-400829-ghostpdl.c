z_jbig2decode(i_ctx_t * i_ctx_p)
{
    os_ptr op = osp;
    ref *sop = NULL;
    s_jbig2_global_data_t *gref;
    stream_jbig2decode_state state;

    /* Extract the global context reference, if any, from the parameter
       dictionary and embed it in our stream state. The original object
       ref is under the JBIG2Globals key.
       We expect the postscript code to resolve this and call
       z_jbig2makeglobalctx() below to create an astruct wrapping the
       global decoder data and store it under the .jbig2globalctx key
     */
    s_jbig2decode_set_global_data((stream_state*)&state, NULL);
    if (r_has_type(op, t_dictionary)) {
        check_dict_read(*op);
        if ( dict_find_string(op, ".jbig2globalctx", &sop) > 0) {
            gref = r_ptr(sop, s_jbig2_global_data_t);
            s_jbig2decode_set_global_data((stream_state*)&state, gref);
        }
    }

    /* we pass npop=0, since we've no arguments left to consume */
    return filter_read(i_ctx_p, 0, &s_jbig2decode_template,
                       (stream_state *) & state, (sop ? r_space(sop) : 0));
}