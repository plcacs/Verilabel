key_share_send_params(gnutls_session_t session,
		      gnutls_buffer_st * extdata)
{
	unsigned i;
	int ret;
	unsigned char *lengthp;
	unsigned int cur_length;
	unsigned int generated = 0;
	const gnutls_group_entry_st *group;
	const version_entry_st *ver;

	/* this extension is only being sent on client side */
	if (session->security_parameters.entity == GNUTLS_CLIENT) {
		ver = _gnutls_version_max(session);
		if (unlikely(ver == NULL || ver->key_shares == 0))
			return 0;

		if (!have_creds_for_tls13(session))
			return 0;

		/* write the total length later */
		lengthp = &extdata->data[extdata->length];

		ret =
		    _gnutls_buffer_append_prefix(extdata, 16, 0);
		if (ret < 0)
			return gnutls_assert_val(ret);

		cur_length = extdata->length;

		if (session->internals.hsk_flags & HSK_HRR_RECEIVED) { /* we know the group */
			group = get_group(session);
			if (unlikely(group == NULL))
				return gnutls_assert_val(GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER);

			ret = client_gen_key_share(session, group, extdata);
			if (ret == GNUTLS_E_INT_RET_0)
				return gnutls_assert_val(GNUTLS_E_NO_COMMON_KEY_SHARE);
			if (ret < 0)
				return gnutls_assert_val(ret);
		} else {
			gnutls_pk_algorithm_t selected_groups[3];
			unsigned max_groups = 2; /* GNUTLS_KEY_SHARE_TOP2 */

			if (session->internals.flags & GNUTLS_KEY_SHARE_TOP)
				max_groups = 1;
			else if (session->internals.flags & GNUTLS_KEY_SHARE_TOP3)
				max_groups = 3;

			assert(max_groups <= sizeof(selected_groups)/sizeof(selected_groups[0]));

			/* generate key shares for out top-(max_groups) groups
			 * if they are of different PK type. */
			for (i = 0; i < session->internals.priorities->groups.size; i++) {
				group = session->internals.priorities->groups.entry[i];

				if (generated == 1 && group->pk == selected_groups[0])
					continue;
				else if (generated == 2 && (group->pk == selected_groups[1] || group->pk == selected_groups[0]))
					continue;

				selected_groups[generated] = group->pk;

				ret = client_gen_key_share(session, group, extdata);
				if (ret == GNUTLS_E_INT_RET_0)
					continue; /* no key share for this algorithm */
				if (ret < 0)
					return gnutls_assert_val(ret);

				generated++;

				if (generated >= max_groups)
					break;
			}
		}

		/* copy actual length */
		_gnutls_write_uint16(extdata->length - cur_length, lengthp);

	} else { /* server */
		ver = get_version(session);
		if (unlikely(ver == NULL || ver->key_shares == 0))
			return gnutls_assert_val(0);

		if (_gnutls_ext_get_msg(session) == GNUTLS_EXT_FLAG_HRR) {
			group = session->internals.cand_group;

			if (group == NULL)
				return gnutls_assert_val(GNUTLS_E_NO_COMMON_KEY_SHARE);

			_gnutls_session_group_set(session, group);

			_gnutls_handshake_log("EXT[%p]: requesting retry with group %s\n", session, group->name);
			ret =
			    _gnutls_buffer_append_prefix(extdata, 16, group->tls_id);
			if (ret < 0)
				return gnutls_assert_val(ret);
		} else {
			/* if we are negotiating PSK without DH, do not send a key share */
			if ((session->internals.hsk_flags & HSK_PSK_SELECTED) &&
			    (session->internals.hsk_flags & HSK_PSK_KE_MODE_PSK))
				return gnutls_assert_val(0);

			group = get_group(session);
			if (unlikely(group == NULL))
				return gnutls_assert_val(GNUTLS_E_RECEIVED_ILLEGAL_PARAMETER);

			ret = server_gen_key_share(session, group, extdata);
			if (ret < 0)
				return gnutls_assert_val(ret);
		}

		session->internals.hsk_flags |= HSK_KEY_SHARE_SENT;
	}

	return 0;
}