static int diffie_hellman_sha256(LIBSSH2_SESSION *session,
                                 _libssh2_bn *g,
                                 _libssh2_bn *p,
                                 int group_order,
                                 unsigned char packet_type_init,
                                 unsigned char packet_type_reply,
                                 unsigned char *midhash,
                                 unsigned long midhash_len,
                                 kmdhgGPshakex_state_t *exchange_state)
{
    int ret = 0;
    int rc;
    libssh2_sha256_ctx exchange_hash_ctx;

    if (exchange_state->state == libssh2_NB_state_idle) {
        /* Setup initial values */
        exchange_state->e_packet = NULL;
        exchange_state->s_packet = NULL;
        exchange_state->k_value = NULL;
        exchange_state->ctx = _libssh2_bn_ctx_new();
        exchange_state->x = _libssh2_bn_init(); /* Random from client */
        exchange_state->e = _libssh2_bn_init(); /* g^x mod p */
        exchange_state->f = _libssh2_bn_init_from_bin(); /* g^(Random from server) mod p */
        exchange_state->k = _libssh2_bn_init(); /* The shared secret: f^x mod p */

        /* Zero the whole thing out */
        memset(&exchange_state->req_state, 0, sizeof(packet_require_state_t));

        /* Generate x and e */
        _libssh2_bn_rand(exchange_state->x, group_order, 0, -1);
        _libssh2_bn_mod_exp(exchange_state->e, g, exchange_state->x, p,
                            exchange_state->ctx);

        /* Send KEX init */
        /* packet_type(1) + String Length(4) + leading 0(1) */
        exchange_state->e_packet_len =
            _libssh2_bn_bytes(exchange_state->e) + 6;
        if (_libssh2_bn_bits(exchange_state->e) % 8) {
            /* Leading 00 not needed */
            exchange_state->e_packet_len--;
        }

        exchange_state->e_packet =
            LIBSSH2_ALLOC(session, exchange_state->e_packet_len);
        if (!exchange_state->e_packet) {
            ret = _libssh2_error(session, LIBSSH2_ERROR_ALLOC,
                                 "Out of memory error");
            goto clean_exit;
        }
        exchange_state->e_packet[0] = packet_type_init;
        _libssh2_htonu32(exchange_state->e_packet + 1,
                         exchange_state->e_packet_len - 5);
        if (_libssh2_bn_bits(exchange_state->e) % 8) {
            _libssh2_bn_to_bin(exchange_state->e,
                               exchange_state->e_packet + 5);
        } else {
            exchange_state->e_packet[5] = 0;
            _libssh2_bn_to_bin(exchange_state->e,
                               exchange_state->e_packet + 6);
        }

        _libssh2_debug(session, LIBSSH2_TRACE_KEX, "Sending KEX packet %d",
                       (int) packet_type_init);
        exchange_state->state = libssh2_NB_state_created;
    }

    if (exchange_state->state == libssh2_NB_state_created) {
        rc = _libssh2_transport_send(session, exchange_state->e_packet,
                                     exchange_state->e_packet_len,
                                     NULL, 0);
        if (rc == LIBSSH2_ERROR_EAGAIN) {
            return rc;
        } else if (rc) {
            ret = _libssh2_error(session, rc,
                                 "Unable to send KEX init message");
            goto clean_exit;
        }
        exchange_state->state = libssh2_NB_state_sent;
    }

    if (exchange_state->state == libssh2_NB_state_sent) {
        if (session->burn_optimistic_kexinit) {
            /* The first KEX packet to come along will be the guess initially
             * sent by the server.  That guess turned out to be wrong so we
             * need to silently ignore it */
            int burn_type;

            _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                           "Waiting for badly guessed KEX packet (to be ignored)");
            burn_type =
                _libssh2_packet_burn(session, &exchange_state->burn_state);
            if (burn_type == LIBSSH2_ERROR_EAGAIN) {
                return burn_type;
            } else if (burn_type <= 0) {
                /* Failed to receive a packet */
                ret = burn_type;
                goto clean_exit;
            }
            session->burn_optimistic_kexinit = 0;

            _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                           "Burnt packet of type: %02x",
                           (unsigned int) burn_type);
        }

        exchange_state->state = libssh2_NB_state_sent1;
    }

    if (exchange_state->state == libssh2_NB_state_sent1) {
        /* Wait for KEX reply */
        rc = _libssh2_packet_require(session, packet_type_reply,
                                     &exchange_state->s_packet,
                                     &exchange_state->s_packet_len, 0, NULL,
                                     0, &exchange_state->req_state);
        if (rc == LIBSSH2_ERROR_EAGAIN) {
            return rc;
        }
        if (rc) {
            ret = _libssh2_error(session, LIBSSH2_ERROR_TIMEOUT,
                                 "Timed out waiting for KEX reply");
            goto clean_exit;
        }

        /* Parse KEXDH_REPLY */
        exchange_state->s = exchange_state->s_packet + 1;

        session->server_hostkey_len = _libssh2_ntohu32(exchange_state->s);
        exchange_state->s += 4;

        if (session->server_hostkey)
            LIBSSH2_FREE(session, session->server_hostkey);

        session->server_hostkey =
            LIBSSH2_ALLOC(session, session->server_hostkey_len);
        if (!session->server_hostkey) {
            ret = _libssh2_error(session, LIBSSH2_ERROR_ALLOC,
                                 "Unable to allocate memory for a copy "
                                 "of the host key");
            goto clean_exit;
        }
        memcpy(session->server_hostkey, exchange_state->s,
               session->server_hostkey_len);
        exchange_state->s += session->server_hostkey_len;

#if LIBSSH2_MD5
        {
            libssh2_md5_ctx fingerprint_ctx;

            if (libssh2_md5_init(&fingerprint_ctx)) {
                libssh2_md5_update(fingerprint_ctx, session->server_hostkey,
                                   session->server_hostkey_len);
                libssh2_md5_final(fingerprint_ctx,
                                  session->server_hostkey_md5);
                session->server_hostkey_md5_valid = TRUE;
            }
            else {
                session->server_hostkey_md5_valid = FALSE;
            }
        }
#ifdef LIBSSH2DEBUG
        {
            char fingerprint[50], *fprint = fingerprint;
            int i;
            for(i = 0; i < 16; i++, fprint += 3) {
                snprintf(fprint, 4, "%02x:", session->server_hostkey_md5[i]);
            }
            *(--fprint) = '\0';
            _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                           "Server's MD5 Fingerprint: %s", fingerprint);
        }
#endif /* LIBSSH2DEBUG */
#endif /* ! LIBSSH2_MD5 */

        {
            libssh2_sha1_ctx fingerprint_ctx;

            if (libssh2_sha1_init(&fingerprint_ctx)) {
                libssh2_sha1_update(fingerprint_ctx, session->server_hostkey,
                                    session->server_hostkey_len);
                libssh2_sha1_final(fingerprint_ctx,
                                   session->server_hostkey_sha1);
                session->server_hostkey_sha1_valid = TRUE;
            }
            else {
                session->server_hostkey_sha1_valid = FALSE;
            }
        }
#ifdef LIBSSH2DEBUG
        {
            char fingerprint[64], *fprint = fingerprint;
            int i;

            for(i = 0; i < 20; i++, fprint += 3) {
                snprintf(fprint, 4, "%02x:", session->server_hostkey_sha1[i]);
            }
            *(--fprint) = '\0';
            _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                           "Server's SHA1 Fingerprint: %s", fingerprint);
        }
#endif /* LIBSSH2DEBUG */

        if (session->hostkey->init(session, session->server_hostkey,
                                   session->server_hostkey_len,
                                   &session->server_hostkey_abstract)) {
            ret = _libssh2_error(session, LIBSSH2_ERROR_HOSTKEY_INIT,
                                 "Unable to initialize hostkey importer");
            goto clean_exit;
        }

        exchange_state->f_value_len = _libssh2_ntohu32(exchange_state->s);
        exchange_state->s += 4;
        exchange_state->f_value = exchange_state->s;
        exchange_state->s += exchange_state->f_value_len;
        _libssh2_bn_from_bin(exchange_state->f, exchange_state->f_value_len,
                             exchange_state->f_value);

        exchange_state->h_sig_len = _libssh2_ntohu32(exchange_state->s);
        exchange_state->s += 4;
        exchange_state->h_sig = exchange_state->s;

        /* Compute the shared secret */
        _libssh2_bn_mod_exp(exchange_state->k, exchange_state->f,
                            exchange_state->x, p, exchange_state->ctx);
        exchange_state->k_value_len = _libssh2_bn_bytes(exchange_state->k) + 5;
        if (_libssh2_bn_bits(exchange_state->k) % 8) {
            /* don't need leading 00 */
            exchange_state->k_value_len--;
        }
        exchange_state->k_value =
            LIBSSH2_ALLOC(session, exchange_state->k_value_len);
        if (!exchange_state->k_value) {
            ret = _libssh2_error(session, LIBSSH2_ERROR_ALLOC,
                                 "Unable to allocate buffer for K");
            goto clean_exit;
        }
        _libssh2_htonu32(exchange_state->k_value,
                         exchange_state->k_value_len - 4);
        if (_libssh2_bn_bits(exchange_state->k) % 8) {
            _libssh2_bn_to_bin(exchange_state->k, exchange_state->k_value + 4);
        } else {
            exchange_state->k_value[4] = 0;
            _libssh2_bn_to_bin(exchange_state->k, exchange_state->k_value + 5);
        }

        exchange_state->exchange_hash = (void*)&exchange_hash_ctx;
        libssh2_sha256_init(&exchange_hash_ctx);

        if (session->local.banner) {
            _libssh2_htonu32(exchange_state->h_sig_comp,
                             strlen((char *) session->local.banner) - 2);
            libssh2_sha256_update(exchange_hash_ctx,
                                  exchange_state->h_sig_comp, 4);
            libssh2_sha256_update(exchange_hash_ctx,
                                  (char *) session->local.banner,
                                  strlen((char *) session->local.banner) - 2);
        } else {
            _libssh2_htonu32(exchange_state->h_sig_comp,
                             sizeof(LIBSSH2_SSH_DEFAULT_BANNER) - 1);
            libssh2_sha256_update(exchange_hash_ctx,
                                  exchange_state->h_sig_comp, 4);
            libssh2_sha256_update(exchange_hash_ctx,
                                  LIBSSH2_SSH_DEFAULT_BANNER,
                                  sizeof(LIBSSH2_SSH_DEFAULT_BANNER) - 1);
        }

        _libssh2_htonu32(exchange_state->h_sig_comp,
                         strlen((char *) session->remote.banner));
        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->h_sig_comp, 4);
        libssh2_sha256_update(exchange_hash_ctx,
                              session->remote.banner,
                              strlen((char *) session->remote.banner));

        _libssh2_htonu32(exchange_state->h_sig_comp,
                         session->local.kexinit_len);
        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->h_sig_comp, 4);
        libssh2_sha256_update(exchange_hash_ctx,
                              session->local.kexinit,
                              session->local.kexinit_len);

        _libssh2_htonu32(exchange_state->h_sig_comp,
                         session->remote.kexinit_len);
        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->h_sig_comp, 4);
        libssh2_sha256_update(exchange_hash_ctx,
                              session->remote.kexinit,
                              session->remote.kexinit_len);

        _libssh2_htonu32(exchange_state->h_sig_comp,
                         session->server_hostkey_len);
        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->h_sig_comp, 4);
        libssh2_sha256_update(exchange_hash_ctx,
                              session->server_hostkey,
                              session->server_hostkey_len);

        if (packet_type_init == SSH_MSG_KEX_DH_GEX_INIT) {
            /* diffie-hellman-group-exchange hashes additional fields */
#ifdef LIBSSH2_DH_GEX_NEW
            _libssh2_htonu32(exchange_state->h_sig_comp,
                             LIBSSH2_DH_GEX_MINGROUP);
            _libssh2_htonu32(exchange_state->h_sig_comp + 4,
                             LIBSSH2_DH_GEX_OPTGROUP);
            _libssh2_htonu32(exchange_state->h_sig_comp + 8,
                             LIBSSH2_DH_GEX_MAXGROUP);
            libssh2_sha256_update(exchange_hash_ctx,
                                  exchange_state->h_sig_comp, 12);
#else
            _libssh2_htonu32(exchange_state->h_sig_comp,
                             LIBSSH2_DH_GEX_OPTGROUP);
            libssh2_sha256_update(exchange_hash_ctx,
                                  exchange_state->h_sig_comp, 4);
#endif
        }

        if (midhash) {
            libssh2_sha256_update(exchange_hash_ctx, midhash,
                                  midhash_len);
        }

        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->e_packet + 1,
                              exchange_state->e_packet_len - 1);

        _libssh2_htonu32(exchange_state->h_sig_comp,
                         exchange_state->f_value_len);
        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->h_sig_comp, 4);
        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->f_value,
                              exchange_state->f_value_len);

        libssh2_sha256_update(exchange_hash_ctx,
                              exchange_state->k_value,
                              exchange_state->k_value_len);

        libssh2_sha256_final(exchange_hash_ctx,
                             exchange_state->h_sig_comp);

        if (session->hostkey->
            sig_verify(session, exchange_state->h_sig,
                       exchange_state->h_sig_len, exchange_state->h_sig_comp,
                       SHA256_DIGEST_LENGTH, &session->server_hostkey_abstract)) {
            ret = _libssh2_error(session, LIBSSH2_ERROR_HOSTKEY_SIGN,
                                 "Unable to verify hostkey signature");
            goto clean_exit;
        }



        _libssh2_debug(session, LIBSSH2_TRACE_KEX, "Sending NEWKEYS message");
        exchange_state->c = SSH_MSG_NEWKEYS;

        exchange_state->state = libssh2_NB_state_sent2;
    }

    if (exchange_state->state == libssh2_NB_state_sent2) {
        rc = _libssh2_transport_send(session, &exchange_state->c, 1, NULL, 0);
        if (rc == LIBSSH2_ERROR_EAGAIN) {
            return rc;
        } else if (rc) {
            ret = _libssh2_error(session, rc, "Unable to send NEWKEYS message");
            goto clean_exit;
        }

        exchange_state->state = libssh2_NB_state_sent3;
    }

    if (exchange_state->state == libssh2_NB_state_sent3) {
        rc = _libssh2_packet_require(session, SSH_MSG_NEWKEYS,
                                     &exchange_state->tmp,
                                     &exchange_state->tmp_len, 0, NULL, 0,
                                     &exchange_state->req_state);
        if (rc == LIBSSH2_ERROR_EAGAIN) {
            return rc;
        } else if (rc) {
            ret = _libssh2_error(session, rc, "Timed out waiting for NEWKEYS");
            goto clean_exit;
        }
        /* The first key exchange has been performed,
           switch to active crypt/comp/mac mode */
        session->state |= LIBSSH2_STATE_NEWKEYS;
        _libssh2_debug(session, LIBSSH2_TRACE_KEX, "Received NEWKEYS message");

        /* This will actually end up being just packet_type(1)
           for this packet type anyway */
        LIBSSH2_FREE(session, exchange_state->tmp);

        if (!session->session_id) {
            session->session_id = LIBSSH2_ALLOC(session, SHA256_DIGEST_LENGTH);
            if (!session->session_id) {
                ret = _libssh2_error(session, LIBSSH2_ERROR_ALLOC,
                                     "Unable to allocate buffer for SHA digest");
                goto clean_exit;
            }
            memcpy(session->session_id, exchange_state->h_sig_comp,
                   SHA256_DIGEST_LENGTH);
            session->session_id_len = SHA256_DIGEST_LENGTH;
            _libssh2_debug(session, LIBSSH2_TRACE_KEX, "session_id calculated");
        }

        /* Cleanup any existing cipher */
        if (session->local.crypt->dtor) {
            session->local.crypt->dtor(session,
                                       &session->local.crypt_abstract);
        }

        /* Calculate IV/Secret/Key for each direction */
        if (session->local.crypt->init) {
            unsigned char *iv = NULL, *secret = NULL;
            int free_iv = 0, free_secret = 0;

            LIBSSH2_KEX_METHOD_DIFFIE_HELLMAN_SHA256_HASH(iv,
                                                          session->local.crypt->
                                                          iv_len, "A");
            if (!iv) {
                ret = -1;
                goto clean_exit;
            }
            LIBSSH2_KEX_METHOD_DIFFIE_HELLMAN_SHA256_HASH(secret,
                                                          session->local.crypt->
                                                          secret_len, "C");
            if (!secret) {
                LIBSSH2_FREE(session, iv);
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }
            if (session->local.crypt->
                init(session, session->local.crypt, iv, &free_iv, secret,
                     &free_secret, 1, &session->local.crypt_abstract)) {
                LIBSSH2_FREE(session, iv);
                LIBSSH2_FREE(session, secret);
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }

            if (free_iv) {
                memset(iv, 0, session->local.crypt->iv_len);
                LIBSSH2_FREE(session, iv);
            }

            if (free_secret) {
                memset(secret, 0, session->local.crypt->secret_len);
                LIBSSH2_FREE(session, secret);
            }
        }
        _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                       "Client to Server IV and Key calculated");

        if (session->remote.crypt->dtor) {
            /* Cleanup any existing cipher */
            session->remote.crypt->dtor(session,
                                        &session->remote.crypt_abstract);
        }

        if (session->remote.crypt->init) {
            unsigned char *iv = NULL, *secret = NULL;
            int free_iv = 0, free_secret = 0;

            LIBSSH2_KEX_METHOD_DIFFIE_HELLMAN_SHA256_HASH(iv,
                                                          session->remote.crypt->
                                                          iv_len, "B");
            if (!iv) {
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }
            LIBSSH2_KEX_METHOD_DIFFIE_HELLMAN_SHA256_HASH(secret,
                                                          session->remote.crypt->
                                                          secret_len, "D");
            if (!secret) {
                LIBSSH2_FREE(session, iv);
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }
            if (session->remote.crypt->
                init(session, session->remote.crypt, iv, &free_iv, secret,
                     &free_secret, 0, &session->remote.crypt_abstract)) {
                LIBSSH2_FREE(session, iv);
                LIBSSH2_FREE(session, secret);
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }

            if (free_iv) {
                memset(iv, 0, session->remote.crypt->iv_len);
                LIBSSH2_FREE(session, iv);
            }

            if (free_secret) {
                memset(secret, 0, session->remote.crypt->secret_len);
                LIBSSH2_FREE(session, secret);
            }
        }
        _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                       "Server to Client IV and Key calculated");

        if (session->local.mac->dtor) {
            session->local.mac->dtor(session, &session->local.mac_abstract);
        }

        if (session->local.mac->init) {
            unsigned char *key = NULL;
            int free_key = 0;

            LIBSSH2_KEX_METHOD_DIFFIE_HELLMAN_SHA256_HASH(key,
                                                          session->local.mac->
                                                          key_len, "E");
            if (!key) {
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }
            session->local.mac->init(session, key, &free_key,
                                     &session->local.mac_abstract);

            if (free_key) {
                memset(key, 0, session->local.mac->key_len);
                LIBSSH2_FREE(session, key);
            }
        }
        _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                       "Client to Server HMAC Key calculated");

        if (session->remote.mac->dtor) {
            session->remote.mac->dtor(session, &session->remote.mac_abstract);
        }

        if (session->remote.mac->init) {
            unsigned char *key = NULL;
            int free_key = 0;

            LIBSSH2_KEX_METHOD_DIFFIE_HELLMAN_SHA256_HASH(key,
                                                          session->remote.mac->
                                                          key_len, "F");
            if (!key) {
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }
            session->remote.mac->init(session, key, &free_key,
                                      &session->remote.mac_abstract);

            if (free_key) {
                memset(key, 0, session->remote.mac->key_len);
                LIBSSH2_FREE(session, key);
            }
        }
        _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                       "Server to Client HMAC Key calculated");

        /* Initialize compression for each direction */

        /* Cleanup any existing compression */
        if (session->local.comp && session->local.comp->dtor) {
            session->local.comp->dtor(session, 1,
                                      &session->local.comp_abstract);
        }

        if (session->local.comp && session->local.comp->init) {
            if (session->local.comp->init(session, 1,
                                          &session->local.comp_abstract)) {
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }
        }
        _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                       "Client to Server compression initialized");

        if (session->remote.comp && session->remote.comp->dtor) {
            session->remote.comp->dtor(session, 0,
                                       &session->remote.comp_abstract);
        }

        if (session->remote.comp && session->remote.comp->init) {
            if (session->remote.comp->init(session, 0,
                                           &session->remote.comp_abstract)) {
                ret = LIBSSH2_ERROR_KEX_FAILURE;
                goto clean_exit;
            }
        }
        _libssh2_debug(session, LIBSSH2_TRACE_KEX,
                       "Server to Client compression initialized");

    }

  clean_exit:
    _libssh2_bn_free(exchange_state->x);
    exchange_state->x = NULL;
    _libssh2_bn_free(exchange_state->e);
    exchange_state->e = NULL;
    _libssh2_bn_free(exchange_state->f);
    exchange_state->f = NULL;
    _libssh2_bn_free(exchange_state->k);
    exchange_state->k = NULL;
    _libssh2_bn_ctx_free(exchange_state->ctx);
    exchange_state->ctx = NULL;

    if (exchange_state->e_packet) {
        LIBSSH2_FREE(session, exchange_state->e_packet);
        exchange_state->e_packet = NULL;
    }

    if (exchange_state->s_packet) {
        LIBSSH2_FREE(session, exchange_state->s_packet);
        exchange_state->s_packet = NULL;
    }

    if (exchange_state->k_value) {
        LIBSSH2_FREE(session, exchange_state->k_value);
        exchange_state->k_value = NULL;
    }

    exchange_state->state = libssh2_NB_state_idle;

    return ret;
}