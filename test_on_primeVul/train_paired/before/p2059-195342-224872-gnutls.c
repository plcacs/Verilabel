cdk_error_t cdk_pkt_read(cdk_stream_t inp, cdk_packet_t pkt)
{
	int ctb, is_newctb;
	int pkttype;
	size_t pktlen = 0, pktsize = 0, is_partial = 0;
	cdk_error_t rc;

	if (!inp || !pkt)
		return CDK_Inv_Value;

	ctb = cdk_stream_getc(inp);
	if (cdk_stream_eof(inp) || ctb == EOF)
		return CDK_EOF;
	else if (!ctb)
		return gnutls_assert_val(CDK_Inv_Packet);

	pktsize++;
	if (!(ctb & 0x80)) {
		_cdk_log_info("cdk_pkt_read: no openpgp data found. "
			      "(ctb=%02X; fpos=%02X)\n", (int) ctb,
			      (int) cdk_stream_tell(inp));
		return gnutls_assert_val(CDK_Inv_Packet);
	}

	if (ctb & 0x40) {	/* RFC2440 packet format. */
		pkttype = ctb & 0x3f;
		is_newctb = 1;
	} else {		/* the old RFC1991 packet format. */

		pkttype = ctb & 0x3f;
		pkttype >>= 2;
		is_newctb = 0;
	}

	if (pkttype > 63) {
		_cdk_log_info("cdk_pkt_read: unknown type %d\n", pkttype);
		return gnutls_assert_val(CDK_Inv_Packet);
	}

	if (is_newctb)
		read_new_length(inp, &pktlen, &pktsize, &is_partial);
	else
		read_old_length(inp, ctb, &pktlen, &pktsize);

	pkt->pkttype = pkttype;
	pkt->pktlen = pktlen;
	pkt->pktsize = pktsize + pktlen;
	pkt->old_ctb = is_newctb ? 0 : 1;

	rc = 0;
	switch (pkt->pkttype) {
	case CDK_PKT_ATTRIBUTE:
#define NAME_SIZE (pkt->pktlen + 16 + 1)
		pkt->pkt.user_id = cdk_calloc(1, sizeof *pkt->pkt.user_id
					      + NAME_SIZE);
		if (!pkt->pkt.user_id)
			return gnutls_assert_val(CDK_Out_Of_Core);
		pkt->pkt.user_id->name =
		    (char *) pkt->pkt.user_id + sizeof(*pkt->pkt.user_id);

		rc = read_attribute(inp, pktlen, pkt->pkt.user_id,
				    NAME_SIZE);
		pkt->pkttype = CDK_PKT_ATTRIBUTE;
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_USER_ID:
		pkt->pkt.user_id = cdk_calloc(1, sizeof *pkt->pkt.user_id
					      + pkt->pktlen + 1);
		if (!pkt->pkt.user_id)
			return gnutls_assert_val(CDK_Out_Of_Core);
		pkt->pkt.user_id->name =
		    (char *) pkt->pkt.user_id + sizeof(*pkt->pkt.user_id);
		rc = read_user_id(inp, pktlen, pkt->pkt.user_id);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_PUBLIC_KEY:
		pkt->pkt.public_key =
		    cdk_calloc(1, sizeof *pkt->pkt.public_key);
		if (!pkt->pkt.public_key)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_public_key(inp, pktlen, pkt->pkt.public_key);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_PUBLIC_SUBKEY:
		pkt->pkt.public_key =
		    cdk_calloc(1, sizeof *pkt->pkt.public_key);
		if (!pkt->pkt.public_key)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_public_subkey(inp, pktlen, pkt->pkt.public_key);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_SECRET_KEY:
		pkt->pkt.secret_key =
		    cdk_calloc(1, sizeof *pkt->pkt.secret_key);
		if (!pkt->pkt.secret_key)
			return gnutls_assert_val(CDK_Out_Of_Core);
		pkt->pkt.secret_key->pk = cdk_calloc(1,
						     sizeof *pkt->pkt.
						     secret_key->pk);
		if (!pkt->pkt.secret_key->pk)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_secret_key(inp, pktlen, pkt->pkt.secret_key);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_SECRET_SUBKEY:
		pkt->pkt.secret_key =
		    cdk_calloc(1, sizeof *pkt->pkt.secret_key);
		if (!pkt->pkt.secret_key)
			return gnutls_assert_val(CDK_Out_Of_Core);
		pkt->pkt.secret_key->pk = cdk_calloc(1,
						     sizeof *pkt->pkt.
						     secret_key->pk);
		if (!pkt->pkt.secret_key->pk)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_secret_subkey(inp, pktlen, pkt->pkt.secret_key);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_LITERAL:
		pkt->pkt.literal = cdk_calloc(1, sizeof *pkt->pkt.literal);
		if (!pkt->pkt.literal)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_literal(inp, pktlen, &pkt->pkt.literal,
				  is_partial);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_ONEPASS_SIG:
		pkt->pkt.onepass_sig =
		    cdk_calloc(1, sizeof *pkt->pkt.onepass_sig);
		if (!pkt->pkt.onepass_sig)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_onepass_sig(inp, pktlen, pkt->pkt.onepass_sig);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_SIGNATURE:
		pkt->pkt.signature =
		    cdk_calloc(1, sizeof *pkt->pkt.signature);
		if (!pkt->pkt.signature)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_signature(inp, pktlen, pkt->pkt.signature);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_PUBKEY_ENC:
		pkt->pkt.pubkey_enc =
		    cdk_calloc(1, sizeof *pkt->pkt.pubkey_enc);
		if (!pkt->pkt.pubkey_enc)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_pubkey_enc(inp, pktlen, pkt->pkt.pubkey_enc);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_COMPRESSED:
		pkt->pkt.compressed =
		    cdk_calloc(1, sizeof *pkt->pkt.compressed);
		if (!pkt->pkt.compressed)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_compressed(inp, pktlen, pkt->pkt.compressed);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	case CDK_PKT_MDC:
		pkt->pkt.mdc = cdk_calloc(1, sizeof *pkt->pkt.mdc);
		if (!pkt->pkt.mdc)
			return gnutls_assert_val(CDK_Out_Of_Core);
		rc = read_mdc(inp, pkt->pkt.mdc);
		if (rc)
			return gnutls_assert_val(rc);
		break;

	default:
		/* Skip all packets we don't understand */
		rc = skip_packet(inp, pktlen);
		if (rc)
			return gnutls_assert_val(rc);
		break;
	}

	return rc;
}