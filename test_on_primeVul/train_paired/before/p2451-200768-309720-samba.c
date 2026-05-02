static struct tevent_req *dns_process_send(TALLOC_CTX *mem_ctx,
					   struct tevent_context *ev,
					   struct dns_server *dns,
					   DATA_BLOB *in)
{
	struct tevent_req *req, *subreq;
	struct dns_process_state *state;
	enum ndr_err_code ndr_err;
	WERROR ret;
	const char *forwarder = lpcfg_dns_forwarder(dns->task->lp_ctx);
	req = tevent_req_create(mem_ctx, &state, struct dns_process_state);
	if (req == NULL) {
		return NULL;
	}
	state->in = in;

	state->dns = dns;

	if (in->length < 12) {
		tevent_req_werror(req, WERR_INVALID_PARAM);
		return tevent_req_post(req, ev);
	}
	dump_data_dbgc(DBGC_DNS, 8, in->data, in->length);

	ndr_err = ndr_pull_struct_blob(
		in, state, &state->in_packet,
		(ndr_pull_flags_fn_t)ndr_pull_dns_name_packet);

	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		state->dns_err = DNS_RCODE_FORMERR;
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}
	if (DEBUGLVLC(DBGC_DNS, 8)) {
		NDR_PRINT_DEBUGC(DBGC_DNS, dns_name_packet, &state->in_packet);
	}

	ret = dns_verify_tsig(dns, state, &state->state, &state->in_packet, in);
	if (!W_ERROR_IS_OK(ret)) {
		DEBUG(1, ("Failed to verify TSIG!\n"));
		state->dns_err = werr_to_dns_err(ret);
		tevent_req_done(req);
		return tevent_req_post(req, ev);
	}

	state->state.flags = state->in_packet.operation;
	state->state.flags |= DNS_FLAG_REPLY;

	
	if (forwarder && *forwarder) {
		state->state.flags |= DNS_FLAG_RECURSION_AVAIL;
	}

	state->out_packet = state->in_packet;

	switch (state->in_packet.operation & DNS_OPCODE) {
	case DNS_OPCODE_QUERY:
		subreq = dns_server_process_query_send(
			state, ev, dns, &state->state, &state->in_packet);
		if (tevent_req_nomem(subreq, req)) {
			return tevent_req_post(req, ev);
		}
		tevent_req_set_callback(subreq, dns_process_done, req);
		return req;
	case DNS_OPCODE_UPDATE:
		ret = dns_server_process_update(
			dns, &state->state, state, &state->in_packet,
			&state->out_packet.answers, &state->out_packet.ancount,
			&state->out_packet.nsrecs,  &state->out_packet.nscount,
			&state->out_packet.additional,
			&state->out_packet.arcount);
		break;
	default:
		ret = WERR_DNS_ERROR_RCODE_NOT_IMPLEMENTED;
	}
	if (!W_ERROR_IS_OK(ret)) {
		state->dns_err = werr_to_dns_err(ret);
	}
	tevent_req_done(req);
	return tevent_req_post(req, ev);
}