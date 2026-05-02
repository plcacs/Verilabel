struct sctp_chunk *sctp_make_strreset_req(
					const struct sctp_association *asoc,
					__u16 stream_num, __be16 *stream_list,
					bool out, bool in)
{
	__u16 stream_len = stream_num * sizeof(__u16);
	struct sctp_strreset_outreq outreq;
	struct sctp_strreset_inreq inreq;
	struct sctp_chunk *retval;
	__u16 outlen, inlen;

	outlen = (sizeof(outreq) + stream_len) * out;
	inlen = (sizeof(inreq) + stream_len) * in;

	retval = sctp_make_reconf(asoc, outlen + inlen);
	if (!retval)
		return NULL;

	if (outlen) {
		outreq.param_hdr.type = SCTP_PARAM_RESET_OUT_REQUEST;
		outreq.param_hdr.length = htons(outlen);
		outreq.request_seq = htonl(asoc->strreset_outseq);
		outreq.response_seq = htonl(asoc->strreset_inseq - 1);
		outreq.send_reset_at_tsn = htonl(asoc->next_tsn - 1);

		sctp_addto_chunk(retval, sizeof(outreq), &outreq);

		if (stream_len)
			sctp_addto_chunk(retval, stream_len, stream_list);
	}

	if (inlen) {
		inreq.param_hdr.type = SCTP_PARAM_RESET_IN_REQUEST;
		inreq.param_hdr.length = htons(inlen);
		inreq.request_seq = htonl(asoc->strreset_outseq + out);

		sctp_addto_chunk(retval, sizeof(inreq), &inreq);

		if (stream_len)
			sctp_addto_chunk(retval, stream_len, stream_list);
	}

	return retval;
}