int gg_change_status_descr(struct gg_session *sess, int status, const char *descr)
{
	struct gg_new_status80 p;
	char *gen_descr = NULL;
	int descr_len = 0;
	int descr_null_len = 0;
	int res;

	gg_debug_session(sess, GG_DEBUG_FUNCTION, "** gg_change_status_descr(%p, %d, \"%s\");\n", sess, status, descr);

	if (!sess) {
		errno = EFAULT;
		return -1;
	}

	if (sess->state != GG_STATE_CONNECTED) {
		errno = ENOTCONN;
		return -1;
	}

	sess->status = status;

	if (descr != NULL && sess->encoding != GG_ENCODING_UTF8) {
		descr = gen_descr = gg_encoding_convert(descr, GG_ENCODING_CP1250, GG_ENCODING_UTF8, -1, -1);

		if (!gen_descr)
			return -1;
	}

	if (descr) {
		descr_len = strlen(descr);

		if (descr_len > GG_STATUS_DESCR_MAXSIZE)
			descr_len = GG_STATUS_DESCR_MAXSIZE;

		/* XXX pamiętać o tym, żeby nie ucinać w środku znaku utf-8 */
	} else {
		descr = "";
	}

	p.status		= gg_fix32(status);
	p.flags			= gg_fix32(sess->status_flags);
	p.description_size	= gg_fix32(descr_len);

	if (sess->protocol_version >= GG_PROTOCOL_110) {
		p.flags = gg_fix32(0x00000014);
		descr_null_len = 1;
	}

	res = gg_send_packet(sess, GG_NEW_STATUS80, 
			&p, sizeof(p), 
			descr, descr_len,
			"\x00", descr_null_len,
			NULL);

	free(gen_descr);

	if (GG_S_NA(status)) {
		sess->state = GG_STATE_DISCONNECTING;
		sess->timeout = GG_TIMEOUT_DISCONNECT;
	}

	return res;
}