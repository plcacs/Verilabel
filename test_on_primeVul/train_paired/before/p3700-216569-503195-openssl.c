SSL_CTX *SSL_CTX_new(const SSL_METHOD *meth)
{
    SSL_CTX *ret = NULL;

    if (meth == NULL) {
        SSLerr(SSL_F_SSL_CTX_NEW, SSL_R_NULL_SSL_METHOD_PASSED);
        return (NULL);
    }
#ifdef OPENSSL_FIPS
    if (FIPS_mode() && (meth->version < TLS1_VERSION)) {
        SSLerr(SSL_F_SSL_CTX_NEW, SSL_R_ONLY_TLS_ALLOWED_IN_FIPS_MODE);
        return NULL;
    }
#endif

    if (SSL_get_ex_data_X509_STORE_CTX_idx() < 0) {
        SSLerr(SSL_F_SSL_CTX_NEW, SSL_R_X509_VERIFICATION_SETUP_PROBLEMS);
        goto err;
    }
    ret = (SSL_CTX *)OPENSSL_malloc(sizeof(SSL_CTX));
    if (ret == NULL)
        goto err;

    memset(ret, 0, sizeof(SSL_CTX));

    ret->method = meth;

    ret->cert_store = NULL;
    ret->session_cache_mode = SSL_SESS_CACHE_SERVER;
    ret->session_cache_size = SSL_SESSION_CACHE_MAX_SIZE_DEFAULT;
    ret->session_cache_head = NULL;
    ret->session_cache_tail = NULL;

    /* We take the system default */
    ret->session_timeout = meth->get_timeout();

    ret->new_session_cb = 0;
    ret->remove_session_cb = 0;
    ret->get_session_cb = 0;
    ret->generate_session_id = 0;

    memset((char *)&ret->stats, 0, sizeof(ret->stats));

    ret->references = 1;
    ret->quiet_shutdown = 0;

/*  ret->cipher=NULL;*/
/*-
    ret->s2->challenge=NULL;
    ret->master_key=NULL;
    ret->key_arg=NULL;
    ret->s2->conn_id=NULL; */

    ret->info_callback = NULL;

    ret->app_verify_callback = 0;
    ret->app_verify_arg = NULL;

    ret->max_cert_list = SSL_MAX_CERT_LIST_DEFAULT;
    ret->read_ahead = 0;
    ret->msg_callback = 0;
    ret->msg_callback_arg = NULL;
    ret->verify_mode = SSL_VERIFY_NONE;
#if 0
    ret->verify_depth = -1;     /* Don't impose a limit (but x509_lu.c does) */
#endif
    ret->sid_ctx_length = 0;
    ret->default_verify_callback = NULL;
    if ((ret->cert = ssl_cert_new()) == NULL)
        goto err;

    ret->default_passwd_callback = 0;
    ret->default_passwd_callback_userdata = NULL;
    ret->client_cert_cb = 0;
    ret->app_gen_cookie_cb = 0;
    ret->app_verify_cookie_cb = 0;

    ret->sessions = lh_SSL_SESSION_new();
    if (ret->sessions == NULL)
        goto err;
    ret->cert_store = X509_STORE_new();
    if (ret->cert_store == NULL)
        goto err;

    ssl_create_cipher_list(ret->method,
                           &ret->cipher_list, &ret->cipher_list_by_id,
                           meth->version ==
                           SSL2_VERSION ? "SSLv2" : SSL_DEFAULT_CIPHER_LIST);
    if (ret->cipher_list == NULL || sk_SSL_CIPHER_num(ret->cipher_list) <= 0) {
        SSLerr(SSL_F_SSL_CTX_NEW, SSL_R_LIBRARY_HAS_NO_CIPHERS);
        goto err2;
    }

    ret->param = X509_VERIFY_PARAM_new();
    if (!ret->param)
        goto err;

    if ((ret->rsa_md5 = EVP_get_digestbyname("ssl2-md5")) == NULL) {
        SSLerr(SSL_F_SSL_CTX_NEW, SSL_R_UNABLE_TO_LOAD_SSL2_MD5_ROUTINES);
        goto err2;
    }
    if ((ret->md5 = EVP_get_digestbyname("ssl3-md5")) == NULL) {
        SSLerr(SSL_F_SSL_CTX_NEW, SSL_R_UNABLE_TO_LOAD_SSL3_MD5_ROUTINES);
        goto err2;
    }
    if ((ret->sha1 = EVP_get_digestbyname("ssl3-sha1")) == NULL) {
        SSLerr(SSL_F_SSL_CTX_NEW, SSL_R_UNABLE_TO_LOAD_SSL3_SHA1_ROUTINES);
        goto err2;
    }

    if ((ret->client_CA = sk_X509_NAME_new_null()) == NULL)
        goto err;

    CRYPTO_new_ex_data(CRYPTO_EX_INDEX_SSL_CTX, ret, &ret->ex_data);

    ret->extra_certs = NULL;
    /* No compression for DTLS */
    if (meth->version != DTLS1_VERSION)
        ret->comp_methods = SSL_COMP_get_compression_methods();

    ret->max_send_fragment = SSL3_RT_MAX_PLAIN_LENGTH;

#ifndef OPENSSL_NO_TLSEXT
    ret->tlsext_servername_callback = 0;
    ret->tlsext_servername_arg = NULL;
    /* Setup RFC4507 ticket keys */
    if ((RAND_pseudo_bytes(ret->tlsext_tick_key_name, 16) <= 0)
        || (RAND_bytes(ret->tlsext_tick_hmac_key, 16) <= 0)
        || (RAND_bytes(ret->tlsext_tick_aes_key, 16) <= 0))
        ret->options |= SSL_OP_NO_TICKET;

    ret->tlsext_status_cb = 0;
    ret->tlsext_status_arg = NULL;

# ifndef OPENSSL_NO_NEXTPROTONEG
    ret->next_protos_advertised_cb = 0;
    ret->next_proto_select_cb = 0;
# endif
#endif
#ifndef OPENSSL_NO_PSK
    ret->psk_identity_hint = NULL;
    ret->psk_client_callback = NULL;
    ret->psk_server_callback = NULL;
#endif
#ifndef OPENSSL_NO_SRP
    SSL_CTX_SRP_CTX_init(ret);
#endif
#ifndef OPENSSL_NO_BUF_FREELISTS
    ret->freelist_max_len = SSL_MAX_BUF_FREELIST_LEN_DEFAULT;
    ret->rbuf_freelist = OPENSSL_malloc(sizeof(SSL3_BUF_FREELIST));
    if (!ret->rbuf_freelist)
        goto err;
    ret->rbuf_freelist->chunklen = 0;
    ret->rbuf_freelist->len = 0;
    ret->rbuf_freelist->head = NULL;
    ret->wbuf_freelist = OPENSSL_malloc(sizeof(SSL3_BUF_FREELIST));
    if (!ret->wbuf_freelist) {
        OPENSSL_free(ret->rbuf_freelist);
        goto err;
    }
    ret->wbuf_freelist->chunklen = 0;
    ret->wbuf_freelist->len = 0;
    ret->wbuf_freelist->head = NULL;
#endif
#ifndef OPENSSL_NO_ENGINE
    ret->client_cert_engine = NULL;
# ifdef OPENSSL_SSL_CLIENT_ENGINE_AUTO
#  define eng_strx(x)     #x
#  define eng_str(x)      eng_strx(x)
    /* Use specific client engine automatically... ignore errors */
    {
        ENGINE *eng;
        eng = ENGINE_by_id(eng_str(OPENSSL_SSL_CLIENT_ENGINE_AUTO));
        if (!eng) {
            ERR_clear_error();
            ENGINE_load_builtin_engines();
            eng = ENGINE_by_id(eng_str(OPENSSL_SSL_CLIENT_ENGINE_AUTO));
        }
        if (!eng || !SSL_CTX_set_client_cert_engine(ret, eng))
            ERR_clear_error();
    }
# endif
#endif
    /*
     * Default is to connect to non-RI servers. When RI is more widely
     * deployed might change this.
     */
    ret->options |= SSL_OP_LEGACY_SERVER_CONNECT;

    return (ret);
 err:
    SSLerr(SSL_F_SSL_CTX_NEW, ERR_R_MALLOC_FAILURE);
 err2:
    if (ret != NULL)
        SSL_CTX_free(ret);
    return (NULL);
}