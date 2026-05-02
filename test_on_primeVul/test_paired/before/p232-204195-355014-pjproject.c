static void parse_rtcp_bye(pjmedia_rtcp_session *sess,
			   const void *pkt,
			   pj_size_t size)
{
    pj_str_t reason = {"-", 1};

    /* Check and get BYE reason */
    if (size > 8) {
	reason.slen = PJ_MIN(sizeof(sess->stat.peer_sdes_buf_),
                             *((pj_uint8_t*)pkt+8));
	pj_memcpy(sess->stat.peer_sdes_buf_, ((pj_uint8_t*)pkt+9),
		  reason.slen);
	reason.ptr = sess->stat.peer_sdes_buf_;
    }

    /* Just print RTCP BYE log */
    PJ_LOG(5, (sess->name, "Received RTCP BYE, reason: %.*s",
	       reason.slen, reason.ptr));
}