client_send_params(gnutls_session_t session,
		   gnutls_buffer_t extdata,
		   const gnutls_psk_client_credentials_t cred)
{
	int ret, ext_offset = 0;
	uint8_t binder_value[MAX_HASH_SIZE];
	size_t spos;
	gnutls_datum_t username = {NULL, 0};
	gnutls_datum_t user_key = {NULL, 0}, rkey = {NULL, 0};
	unsigned client_hello_len;
	unsigned next_idx;
	const mac_entry_st *prf_res = NULL;
	const mac_entry_st *prf_psk = NULL;
	struct timespec cur_time;
	uint32_t ticket_age, ob_ticket_age;
	int free_username = 0;
	psk_auth_info_t info = NULL;
	unsigned psk_id_len = 0;
	unsigned binders_len, binders_pos;
	tls13_ticket_st *ticket = &session->internals.tls13_ticket;

	if (((session->internals.flags & GNUTLS_NO_TICKETS) ||
	    session->internals.tls13_ticket.ticket.data == NULL) &&
	    (!cred || !_gnutls_have_psk_credentials(cred, session))) {

		return 0;
	}

	binders_len = 0;

	/* placeholder to be filled later */
	spos = extdata->length;
	ret = _gnutls_buffer_append_prefix(extdata, 16, 0);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* First, let's see if we have a session ticket to send */
	if (!(session->internals.flags & GNUTLS_NO_TICKETS) &&
	    ticket->ticket.data != NULL) {

		/* We found a session ticket */
		if (unlikely(ticket->prf == NULL)) {
			tls13_ticket_deinit(ticket);
			ret = gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
			goto cleanup;
		}

		prf_res = ticket->prf;

		gnutls_gettime(&cur_time);
		if (unlikely(_gnutls_timespec_cmp(&cur_time,
						  &ticket->arrival_time) < 0)) {
			gnutls_assert();
			tls13_ticket_deinit(ticket);
			goto ignore_ticket;
		}

		/* Check whether the ticket is stale */
		ticket_age = timespec_sub_ms(&cur_time, &ticket->arrival_time);
		if (ticket_age / 1000 > ticket->lifetime) {
			tls13_ticket_deinit(ticket);
			goto ignore_ticket;
		}

		ret = compute_psk_from_ticket(ticket, &rkey);
		if (ret < 0) {
			tls13_ticket_deinit(ticket);
			goto ignore_ticket;
		}

		/* Calculate obfuscated ticket age, in milliseconds, mod 2^32 */
		ob_ticket_age = ticket_age + ticket->age_add;

		if ((ret = _gnutls_buffer_append_data_prefix(extdata, 16,
							     ticket->ticket.data,
							     ticket->ticket.size)) < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* Now append the obfuscated ticket age */
		if ((ret = _gnutls_buffer_append_prefix(extdata, 32, ob_ticket_age)) < 0) {
			gnutls_assert();
			goto cleanup;
		}

		psk_id_len += 6 + ticket->ticket.size;
		binders_len += 1 + _gnutls_mac_get_algo_len(prf_res);
	}

 ignore_ticket:
	if (cred && _gnutls_have_psk_credentials(cred, session)) {
		gnutls_datum_t tkey;

		if (cred->binder_algo == NULL) {
			gnutls_assert();
			ret = gnutls_assert_val(GNUTLS_E_INSUFFICIENT_CREDENTIALS);
			goto cleanup;
		}

		prf_psk = cred->binder_algo;

		ret = _gnutls_find_psk_key(session, cred, &username, &tkey, &free_username);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		if (username.size == 0 || username.size > UINT16_MAX) {
			ret = gnutls_assert_val(GNUTLS_E_INVALID_PASSWORD);
			goto cleanup;
		}

		if (!free_username) {
			/* we need to copy the key */
			ret = _gnutls_set_datum(&user_key, tkey.data, tkey.size);
			if (ret < 0) {
				gnutls_assert();
				goto cleanup;
			}
		} else {
			user_key.data = tkey.data;
			user_key.size = tkey.size;
		}

		ret = _gnutls_auth_info_init(session, GNUTLS_CRD_PSK, sizeof(psk_auth_info_st), 1);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		info = _gnutls_get_auth_info(session, GNUTLS_CRD_PSK);
		assert(info != NULL);

		_gnutls_copy_psk_username(info, &username);

		if ((ret = _gnutls_buffer_append_data_prefix(extdata, 16,
							     username.data,
							     username.size)) < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* Now append the obfuscated ticket age */
		if ((ret = _gnutls_buffer_append_prefix(extdata, 32, 0)) < 0) {
			gnutls_assert();
			goto cleanup;
		}

		psk_id_len += 6 + username.size;
		binders_len += 1 + _gnutls_mac_get_algo_len(prf_psk);
	}

	/* if no tickets or identities to be sent */
	if (psk_id_len == 0) {
		/* reset extensions buffer */
		extdata->length = spos;
		return 0;
	}

	_gnutls_write_uint16(psk_id_len, &extdata->data[spos]);

	binders_pos = extdata->length-spos;
	ext_offset = _gnutls_ext_get_extensions_offset(session);

	/* Compute the binders. extdata->data points to the start
	 * of this client hello. */
	assert(extdata->length >= sizeof(mbuffer_st));
	assert(ext_offset >= (ssize_t)sizeof(mbuffer_st));
	ext_offset -= sizeof(mbuffer_st);
	client_hello_len = extdata->length-sizeof(mbuffer_st);

	next_idx = 0;

	ret = _gnutls_buffer_append_prefix(extdata, 16, binders_len);
	if (ret < 0) {
		gnutls_assert_val(ret);
		goto cleanup;
	}

	if (prf_res && rkey.size > 0) {
		gnutls_datum_t client_hello;

		client_hello.data = extdata->data+sizeof(mbuffer_st);
		client_hello.size = client_hello_len;

		ret = compute_psk_binder(session, prf_res,
					 binders_len, binders_pos,
					 ext_offset, &rkey, &client_hello, 1,
					 binder_value);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* Associate the selected pre-shared key with the session */
		gnutls_free(session->key.binders[next_idx].psk.data);
		session->key.binders[next_idx].psk.data = rkey.data;
		session->key.binders[next_idx].psk.size = rkey.size;
		rkey.data = NULL;

		session->key.binders[next_idx].prf = prf_res;
		session->key.binders[next_idx].resumption = 1;
		session->key.binders[next_idx].idx = next_idx;

		_gnutls_handshake_log("EXT[%p]: sent PSK resumption identity (%d)\n", session, next_idx);

		next_idx++;

		/* Add the binder */
		ret = _gnutls_buffer_append_data_prefix(extdata, 8, binder_value, prf_res->output_size);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		session->internals.hsk_flags |= HSK_TLS13_TICKET_SENT;
	}

	if (prf_psk && user_key.size > 0 && info) {
		gnutls_datum_t client_hello;

		client_hello.data = extdata->data+sizeof(mbuffer_st);
		client_hello.size = client_hello_len;

		ret = compute_psk_binder(session, prf_psk,
					 binders_len, binders_pos,
					 ext_offset, &user_key, &client_hello, 0,
					 binder_value);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}

		/* Associate the selected pre-shared key with the session */
		gnutls_free(session->key.binders[next_idx].psk.data);
		session->key.binders[next_idx].psk.data = user_key.data;
		session->key.binders[next_idx].psk.size = user_key.size;
		user_key.data = NULL;

		session->key.binders[next_idx].prf = prf_psk;
		session->key.binders[next_idx].resumption = 0;
		session->key.binders[next_idx].idx = next_idx;

		_gnutls_handshake_log("EXT[%p]: sent PSK identity '%s' (%d)\n", session, info->username, next_idx);

		next_idx++;

		/* Add the binder */
		ret = _gnutls_buffer_append_data_prefix(extdata, 8, binder_value, prf_psk->output_size);
		if (ret < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}

	ret = 0;

cleanup:
	if (free_username)
		_gnutls_free_datum(&username);

	_gnutls_free_temp_key_datum(&user_key);
	_gnutls_free_temp_key_datum(&rkey);

	return ret;
}