SSL_SESSION *ssl_session_dup(SSL_SESSION *src, int ticket)
{
    SSL_SESSION *dest;

    dest = OPENSSL_malloc(sizeof(*src));
    if (dest == NULL) {
        goto err;
    }
    memcpy(dest, src, sizeof(*dest));

    /*
     * Set the various pointers to NULL so that we can call SSL_SESSION_free in
     * the case of an error whilst halfway through constructing dest
     */
#ifndef OPENSSL_NO_PSK
    dest->psk_identity_hint = NULL;
    dest->psk_identity = NULL;
#endif
    dest->ciphers = NULL;
    dest->tlsext_hostname = NULL;
#ifndef OPENSSL_NO_EC
    dest->tlsext_ecpointformatlist = NULL;
    dest->tlsext_ellipticcurvelist = NULL;
#endif
    dest->tlsext_tick = NULL;
#ifndef OPENSSL_NO_SRP
    dest->srp_username = NULL;
#endif
    memset(&dest->ex_data, 0, sizeof(dest->ex_data));

    /* We deliberately don't copy the prev and next pointers */
    dest->prev = NULL;
    dest->next = NULL;

    dest->references = 1;

    if (src->sess_cert != NULL)
        CRYPTO_add(&src->sess_cert->references, 1, CRYPTO_LOCK_SSL_SESS_CERT);

    if (src->peer != NULL)
        CRYPTO_add(&src->peer->references, 1, CRYPTO_LOCK_X509);

#ifndef OPENSSL_NO_PSK
    if (src->psk_identity_hint) {
        dest->psk_identity_hint = BUF_strdup(src->psk_identity_hint);
        if (dest->psk_identity_hint == NULL) {
            goto err;
        }
    }
    if (src->psk_identity) {
        dest->psk_identity = BUF_strdup(src->psk_identity);
        if (dest->psk_identity == NULL) {
            goto err;
        }
    }
#endif

    if(src->ciphers != NULL) {
        dest->ciphers = sk_SSL_CIPHER_dup(src->ciphers);
        if (dest->ciphers == NULL)
            goto err;
    }

    if (!CRYPTO_dup_ex_data(CRYPTO_EX_INDEX_SSL_SESSION,
                                            &dest->ex_data, &src->ex_data)) {
        goto err;
    }

    if (src->tlsext_hostname) {
        dest->tlsext_hostname = BUF_strdup(src->tlsext_hostname);
        if (dest->tlsext_hostname == NULL) {
            goto err;
        }
    }
#ifndef OPENSSL_NO_EC
    if (src->tlsext_ecpointformatlist) {
        dest->tlsext_ecpointformatlist =
            BUF_memdup(src->tlsext_ecpointformatlist,
                       src->tlsext_ecpointformatlist_length);
        if (dest->tlsext_ecpointformatlist == NULL)
            goto err;
    }
    if (src->tlsext_ellipticcurvelist) {
        dest->tlsext_ellipticcurvelist =
            BUF_memdup(src->tlsext_ellipticcurvelist,
                       src->tlsext_ellipticcurvelist_length);
        if (dest->tlsext_ellipticcurvelist == NULL)
            goto err;
    }
#endif

    if (ticket != 0) {
        dest->tlsext_tick = BUF_memdup(src->tlsext_tick, src->tlsext_ticklen);
        if(dest->tlsext_tick == NULL)
            goto err;
    } else {
        dest->tlsext_tick_lifetime_hint = 0;
        dest->tlsext_ticklen = 0;
    }

#ifndef OPENSSL_NO_SRP
    if (src->srp_username) {
        dest->srp_username = BUF_strdup(src->srp_username);
        if (dest->srp_username == NULL) {
            goto err;
        }
    }
#endif

    return dest;
err:
    SSLerr(SSL_F_SSL_SESSION_DUP, ERR_R_MALLOC_FAILURE);
    SSL_SESSION_free(dest);
    return NULL;
}