_gnutls_server_select_suite(gnutls_session_t session, uint8_t * data,
			    unsigned int datalen)
{
	int ret;
	unsigned int i, j, cipher_suites_size;
	size_t pk_algos_size;
	uint8_t cipher_suites[MAX_CIPHERSUITE_SIZE];
	int retval;
	gnutls_pk_algorithm_t pk_algos[MAX_ALGOS];	/* will hold the pk algorithms
							 * supported by the peer.
							 */

	for (i = 0; i < datalen; i += 2) {
		/* TLS_RENEGO_PROTECTION_REQUEST = { 0x00, 0xff } */
		if (session->internals.priorities.sr != SR_DISABLED &&
		    data[i] == GNUTLS_RENEGO_PROTECTION_REQUEST_MAJOR &&
		    data[i + 1] == GNUTLS_RENEGO_PROTECTION_REQUEST_MINOR) {
			_gnutls_handshake_log
			    ("HSK[%p]: Received safe renegotiation CS\n",
			     session);
			retval = _gnutls_ext_sr_recv_cs(session);
			if (retval < 0) {
				gnutls_assert();
				return retval;
			}
		}

		/* TLS_FALLBACK_SCSV */
		if (data[i] == GNUTLS_FALLBACK_SCSV_MAJOR &&
		    data[i + 1] == GNUTLS_FALLBACK_SCSV_MINOR) {
			_gnutls_handshake_log
			    ("HSK[%p]: Received fallback CS\n",
			     session);

			if (gnutls_protocol_get_version(session) !=
			    GNUTLS_TLS_VERSION_MAX)
				return GNUTLS_E_INAPPROPRIATE_FALLBACK;
		}
	}

	pk_algos_size = MAX_ALGOS;
	ret =
	    server_find_pk_algos_in_ciphersuites(data, datalen, pk_algos,
						 &pk_algos_size);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret =
	    _gnutls_supported_ciphersuites(session, cipher_suites,
					   sizeof(cipher_suites));
	if (ret < 0)
		return gnutls_assert_val(ret);

	cipher_suites_size = ret;

	/* Here we remove any ciphersuite that does not conform
	 * the certificate requested, or to the
	 * authentication requested (e.g. SRP).
	 */
	ret =
	    _gnutls_remove_unwanted_ciphersuites(session, cipher_suites,
						 cipher_suites_size,
						 pk_algos, pk_algos_size);
	if (ret <= 0) {
		gnutls_assert();
		if (ret < 0)
			return ret;
		else
			return GNUTLS_E_UNKNOWN_CIPHER_SUITE;
	}

	cipher_suites_size = ret;

	/* Data length should be zero mod 2 since
	 * every ciphersuite is 2 bytes. (this check is needed
	 * see below).
	 */
	if (datalen % 2 != 0) {
		gnutls_assert();
		return GNUTLS_E_UNEXPECTED_PACKET_LENGTH;
	}

	memset(session->security_parameters.cipher_suite, 0, 2);

	retval = GNUTLS_E_UNKNOWN_CIPHER_SUITE;

	_gnutls_handshake_log
	    ("HSK[%p]: Requested cipher suites[size: %d]: \n", session,
	     (int) datalen);

	if (session->internals.priorities.server_precedence == 0) {
		for (j = 0; j < datalen; j += 2) {
			_gnutls_handshake_log("\t0x%.2x, 0x%.2x %s\n",
					      data[j], data[j + 1],
					      _gnutls_cipher_suite_get_name
					      (&data[j]));
			for (i = 0; i < cipher_suites_size; i += 2) {
				if (memcmp(&cipher_suites[i], &data[j], 2)
				    == 0) {
					_gnutls_handshake_log
					    ("HSK[%p]: Selected cipher suite: %s\n",
					     session,
					     _gnutls_cipher_suite_get_name
					     (&data[j]));
					memcpy(session->
					       security_parameters.
					       cipher_suite,
					       &cipher_suites[i], 2);
					_gnutls_epoch_set_cipher_suite
					    (session, EPOCH_NEXT,
					     session->security_parameters.
					     cipher_suite);


					retval = 0;
					goto finish;
				}
			}
		}
	} else {		/* server selects */

		for (i = 0; i < cipher_suites_size; i += 2) {
			for (j = 0; j < datalen; j += 2) {
				if (memcmp(&cipher_suites[i], &data[j], 2)
				    == 0) {
					_gnutls_handshake_log
					    ("HSK[%p]: Selected cipher suite: %s\n",
					     session,
					     _gnutls_cipher_suite_get_name
					     (&data[j]));
					memcpy(session->
					       security_parameters.
					       cipher_suite,
					       &cipher_suites[i], 2);
					_gnutls_epoch_set_cipher_suite
					    (session, EPOCH_NEXT,
					     session->security_parameters.
					     cipher_suite);


					retval = 0;
					goto finish;
				}
			}
		}
	}
      finish:

	if (retval != 0) {
		gnutls_assert();
		return retval;
	}

	/* check if the credentials (username, public key etc.) are ok
	 */
	if (_gnutls_get_kx_cred
	    (session,
	     _gnutls_cipher_suite_get_kx_algo(session->security_parameters.
					      cipher_suite)) == NULL) {
		gnutls_assert();
		return GNUTLS_E_INSUFFICIENT_CREDENTIALS;
	}


	/* set the mod_auth_st to the appropriate struct
	 * according to the KX algorithm. This is needed since all the
	 * handshake functions are read from there;
	 */
	session->internals.auth_struct =
	    _gnutls_kx_auth_struct(_gnutls_cipher_suite_get_kx_algo
				   (session->security_parameters.
				    cipher_suite));
	if (session->internals.auth_struct == NULL) {

		_gnutls_handshake_log
		    ("HSK[%p]: Cannot find the appropriate handler for the KX algorithm\n",
		     session);
		gnutls_assert();
		return GNUTLS_E_INTERNAL_ERROR;
	}

	return 0;

}