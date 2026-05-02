static void cli_session_setup_gensec_remote_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct cli_session_setup_gensec_state *state =
		tevent_req_data(req,
		struct cli_session_setup_gensec_state);
	NTSTATUS status;

	TALLOC_FREE(state->inbuf);
	TALLOC_FREE(state->recv_iov);

	status = cli_sesssetup_blob_recv(subreq, state, &state->blob_in,
					 &state->inbuf, &state->recv_iov);
	TALLOC_FREE(subreq);
	data_blob_free(&state->blob_out);
	if (!NT_STATUS_IS_OK(status) &&
	    !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED))
	{
		tevent_req_nterror(req, status);
		return;
	}

	if (NT_STATUS_IS_OK(status)) {
		struct smbXcli_session *session = NULL;
		bool is_guest = false;

		if (smbXcli_conn_protocol(state->cli->conn) >= PROTOCOL_SMB2_02) {
			session = state->cli->smb2.session;
		} else {
			session = state->cli->smb1.session;
		}

		is_guest = smbXcli_session_is_guest(session);
		if (is_guest) {
			/*
			 * We can't finish the gensec handshake, we don't
			 * have a negotiated session key.
			 *
			 * So just pretend we are completely done.
			 */
			state->blob_in = data_blob_null;
			state->local_ready = true;
		}

		state->remote_ready = true;
	}

	if (state->local_ready && state->remote_ready) {
		cli_session_setup_gensec_ready(req);
		return;
	}

	cli_session_setup_gensec_local_next(req);
}