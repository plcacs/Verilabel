static gg_action_t gg_handle_tls_negotiation(struct gg_session *sess, struct gg_event *e, enum gg_state_t next_state, enum gg_state_t alt_state, enum gg_state_t alt2_state)
{
	int valid_hostname = 0;

#ifdef GG_CONFIG_HAVE_GNUTLS
	unsigned int status;
	int res;

	gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() GG_STATE_TLS_NEGOTIATION\n");

	for (;;) {
		res = gnutls_handshake(GG_SESSION_GNUTLS(sess));

		if (res == GNUTLS_E_AGAIN) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() TLS handshake GNUTLS_E_AGAIN\n");

			if (gnutls_record_get_direction(GG_SESSION_GNUTLS(sess)) == 0)
				sess->check = GG_CHECK_READ;
			else
				sess->check = GG_CHECK_WRITE;
			sess->timeout = GG_DEFAULT_TIMEOUT;
			return GG_ACTION_WAIT;
		}

		if (res == GNUTLS_E_INTERRUPTED) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() TLS handshake GNUTLS_E_INTERRUPTED\n");
			continue;
		}

		if (res != GNUTLS_E_SUCCESS) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() TLS handshake error: %d, %s\n", res, gnutls_strerror(res));
			e->event.failure = GG_FAILURE_TLS;
			return GG_ACTION_FAIL;
		}

		break;
	}

	gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() TLS negotiation succeded:\n");
	gg_debug_session(sess, GG_DEBUG_MISC, "//   cipher: VERS-%s:%s:%s:%s:COMP-%s\n",
		gnutls_protocol_get_name(gnutls_protocol_get_version(GG_SESSION_GNUTLS(sess))),
		gnutls_cipher_get_name(gnutls_cipher_get(GG_SESSION_GNUTLS(sess))),
		gnutls_kx_get_name(gnutls_kx_get(GG_SESSION_GNUTLS(sess))),
		gnutls_mac_get_name(gnutls_mac_get(GG_SESSION_GNUTLS(sess))),
		gnutls_compression_get_name(gnutls_compression_get(GG_SESSION_GNUTLS(sess))));

	if (gnutls_certificate_type_get(GG_SESSION_GNUTLS(sess)) == GNUTLS_CRT_X509) {
		unsigned int peer_count;
		const gnutls_datum_t *peers;
		gnutls_x509_crt_t cert;

		if (gnutls_x509_crt_init(&cert) == 0) {
			peers = gnutls_certificate_get_peers(GG_SESSION_GNUTLS(sess), &peer_count);

			if (peers != NULL) {
				char buf[256];
				size_t size;

				if (gnutls_x509_crt_import(cert, &peers[0], GNUTLS_X509_FMT_DER) == 0) {
					size = sizeof(buf);
					gnutls_x509_crt_get_dn(cert, buf, &size);
					gg_debug_session(sess, GG_DEBUG_MISC, "//   cert subject: %s\n", buf);
					size = sizeof(buf);
					gnutls_x509_crt_get_issuer_dn(cert, buf, &size);
					gg_debug_session(sess, GG_DEBUG_MISC, "//   cert issuer: %s\n", buf);

					if (gnutls_x509_crt_check_hostname(cert, sess->connect_host) != 0)
						valid_hostname = 1;
				}
			}

			gnutls_x509_crt_deinit(cert);
		}
	}

	res = gnutls_certificate_verify_peers2(GG_SESSION_GNUTLS(sess), &status);

	if (res != 0) {
		gg_debug_session(sess, GG_DEBUG_MISC, "//   WARNING!  unable to verify peer certificate: %d, %s\n", res, gnutls_strerror(res));

		if (sess->ssl_flag == GG_SSL_REQUIRED) {
			e->event.failure = GG_FAILURE_TLS;
			return GG_ACTION_FAIL;
		}
	} else {
		gg_debug_session(sess, GG_DEBUG_MISC, "//   verified peer certificate\n");
	}


#elif defined GG_CONFIG_HAVE_OPENSSL

	X509 *peer;
	int res;

	gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() %s\n", gg_debug_state(sess->state));

	res = SSL_connect(GG_SESSION_OPENSSL(sess));

	if (res <= 0) {
		int err;
		
		err = SSL_get_error(GG_SESSION_OPENSSL(sess), res);

		if (res == 0) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() disconnected during TLS negotiation\n");
			e->event.failure = GG_FAILURE_TLS;
			return GG_ACTION_FAIL;
		}

		if (err == SSL_ERROR_WANT_READ) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() SSL_connect() wants to read\n");

			sess->check = GG_CHECK_READ;
			sess->timeout = GG_DEFAULT_TIMEOUT;
			return GG_ACTION_WAIT;
		} else if (err == SSL_ERROR_WANT_WRITE) {
			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() SSL_connect() wants to write\n");

			sess->check = GG_CHECK_WRITE;
			sess->timeout = GG_DEFAULT_TIMEOUT;
			return GG_ACTION_WAIT;
		} else {
			char buf[256];

			ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));

			gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() SSL_connect() bailed out: %s\n", buf);

			e->event.failure = GG_FAILURE_TLS;
			return GG_ACTION_FAIL;
		}
	}

	gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() TLS negotiation succeded:\n//   cipher: %s\n", SSL_get_cipher_name(GG_SESSION_OPENSSL(sess)));

	peer = SSL_get_peer_certificate(GG_SESSION_OPENSSL(sess));

	if (peer == NULL) {
		gg_debug_session(sess, GG_DEBUG_MISC, "//   WARNING! unable to get peer certificate!\n");

		if (sess->ssl_flag == GG_SSL_REQUIRED) {
			e->event.failure = GG_FAILURE_TLS;
			return GG_ACTION_FAIL;
		}
	} else {
		char buf[256];
		long res;

		X509_NAME_oneline(X509_get_subject_name(peer), buf, sizeof(buf));
		gg_debug_session(sess, GG_DEBUG_MISC, "//   cert subject: %s\n", buf);

		X509_NAME_oneline(X509_get_issuer_name(peer), buf, sizeof(buf));
		gg_debug_session(sess, GG_DEBUG_MISC, "//   cert issuer: %s\n", buf);

		res = SSL_get_verify_result(GG_SESSION_OPENSSL(sess));

		if (res != X509_V_OK) {
			gg_debug_session(sess, GG_DEBUG_MISC, "//   WARNING!  unable to verify peer certificate! res=%ld\n", res);

			if (sess->ssl_flag == GG_SSL_REQUIRED) {
				e->event.failure = GG_FAILURE_TLS;
				return GG_ACTION_FAIL;
			}
		} else {
			gg_debug_session(sess, GG_DEBUG_MISC, "//   verified peer certificate\n");
		}

		if (X509_NAME_get_text_by_NID(X509_get_subject_name(peer), NID_commonName, buf, sizeof(buf)) == -1)
			buf[0] = 0;

		/* Obsługa certyfikatów z wieloznacznikiem */
		if (strchr(buf, '*') == buf && strchr(buf + 1, '*') == NULL) {
			char *tmp;

			tmp = strchr(sess->connect_host, '.');

			if (tmp != NULL)
				valid_hostname = (strcasecmp(tmp, buf + 1) == 0);
		} else {
			valid_hostname = (strcasecmp(sess->connect_host, buf) == 0);
		}
	}

#else

	gg_debug_session(sess, GG_DEBUG_MISC, "// gg_watch_fd() no SSL support\n");
	e->event.failure = GG_FAILURE_TLS;
	return GG_ACTION_FAIL;

#endif

	if (!valid_hostname) {
		gg_debug_session(sess, GG_DEBUG_MISC, "//   WARNING!  unable to verify hostname\n");

		if (sess->ssl_flag == GG_SSL_REQUIRED) {
			e->event.failure = GG_FAILURE_TLS;
			return GG_ACTION_FAIL;
		}
	}

	sess->state = next_state;
	sess->check = GG_CHECK_READ;
	sess->timeout = GG_DEFAULT_TIMEOUT;

	return GG_ACTION_WAIT;
}