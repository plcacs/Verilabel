_gnutls13_recv_async_handshake(gnutls_session_t session)
{
	int ret;
	handshake_buffer_st hsk;
	recv_state_t next_state = RECV_STATE_0;

	/* The following messages are expected asynchronously after
	 * the handshake process is complete */
	if (unlikely(session->internals.handshake_in_progress))
		return gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET);

	do {
		/* the received handshake message has already been pushed into
		 * handshake buffers. As we do not need to use the handshake hash
		 * buffers we call the lower level receive functions */
		ret = _gnutls_handshake_io_recv_int(session, GNUTLS_HANDSHAKE_ANY, &hsk, 0);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
		session->internals.last_handshake_in = hsk.htype;

		ret = _gnutls_call_hook_func(session, hsk.htype, GNUTLS_HOOK_PRE, 1,
					     hsk.data.data, hsk.data.length);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		switch(hsk.htype) {
			case GNUTLS_HANDSHAKE_CERTIFICATE_REQUEST:
				if (!(session->security_parameters.entity == GNUTLS_CLIENT) ||
				    !(session->internals.flags & GNUTLS_POST_HANDSHAKE_AUTH)) {
					ret = gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET);
					goto cleanup;
				}

				_gnutls_buffer_reset(&session->internals.reauth_buffer);

				/* include the handshake headers in reauth buffer */
				ret = _gnutls_buffer_append_data(&session->internals.reauth_buffer,
								 hsk.header, hsk.header_size);
				if (ret < 0) {
					gnutls_assert();
					goto cleanup;
				}

				ret = _gnutls_buffer_append_data(&session->internals.reauth_buffer,
								 hsk.data.data, hsk.data.length);
				if (ret < 0) {
					gnutls_assert();
					goto cleanup;
				}

				if (session->internals.flags & GNUTLS_AUTO_REAUTH) {
					ret = gnutls_reauth(session, 0);
					if (ret == GNUTLS_E_AGAIN || ret == GNUTLS_E_INTERRUPTED) {
						next_state = RECV_STATE_REAUTH;
					} else if (ret < 0) {
						gnutls_assert();
						goto cleanup;
					}
				} else {
					/* Application is expected to handle re-authentication
					 * explicitly.  */
					ret = GNUTLS_E_REAUTH_REQUEST;
				}

				goto cleanup;

			case GNUTLS_HANDSHAKE_KEY_UPDATE:
				ret = _gnutls13_recv_key_update(session, &hsk.data);
				if (ret < 0) {
					gnutls_assert();
					goto cleanup;
				}

				/* Handshake messages MUST NOT span key changes, i.e., we
				 * should not have any other pending handshake messages from
				 * the same record. */
				if (session->internals.handshake_recv_buffer_size != 0) {
					ret = gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET);
					goto cleanup;
				}
				break;
			case GNUTLS_HANDSHAKE_NEW_SESSION_TICKET:
				if (session->security_parameters.entity != GNUTLS_CLIENT) {
					ret = gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET);
					goto cleanup;
				}

				ret = _gnutls13_recv_session_ticket(session, &hsk.data);
				if (ret < 0) {
					gnutls_assert();
					goto cleanup;
				}

				memcpy(session->internals.tls13_ticket.resumption_master_secret,
				       session->key.proto.tls13.ap_rms,
				       session->key.proto.tls13.temp_secret_size);

				session->internals.tls13_ticket.prf = session->security_parameters.prf;
				session->internals.hsk_flags |= HSK_TICKET_RECEIVED;
				break;
			default:
				gnutls_assert();
				ret = GNUTLS_E_UNEXPECTED_PACKET;
				goto cleanup;
		}

		ret = _gnutls_call_hook_func(session, hsk.htype, GNUTLS_HOOK_POST, 1, hsk.data.data, hsk.data.length);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
		_gnutls_handshake_buffer_clear(&hsk);

	} while (_gnutls_record_buffer_get_size(session) > 0);

	session->internals.recv_state = next_state;

	return 0;

 cleanup:
	/* if we have pending/partial handshake data in buffers, ensure that
	 * next read will read handshake data */
	if (_gnutls_record_buffer_get_size(session) > 0)
		session->internals.recv_state = RECV_STATE_ASYNC_HANDSHAKE;
	else
		session->internals.recv_state = next_state;

	_gnutls_handshake_buffer_clear(&hsk);
	return ret;
}