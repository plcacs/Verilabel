int Socket::startSslClient(const std::string &certificate_path, String hostname)
{
    if (isssl) {
        stopSsl();
    }

    ERR_clear_error();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    ctx = SSL_CTX_new(SSLv23_client_method());
#else
    ctx = SSL_CTX_new(TLS_client_method());
#endif

    if (ctx == NULL) {
#ifdef NETDEBUG
        std::cout << thread_id << "Error ssl context is null (check that openssl has been inited)" << std::endl;
#endif
        log_ssl_errors("Error ssl context is null for %s", hostname.c_str());
        return -1;
    }

    //set the timeout for the ssl session
    if (SSL_CTX_set_timeout(ctx, 130l) < 1) {
            SSL_CTX_free(ctx);
            ctx = NULL;
        return -1;
    }

    //load certs
    ERR_clear_error();
    if (certificate_path.length()) {
        if (!SSL_CTX_load_verify_locations(ctx, NULL, certificate_path.c_str())) {
#ifdef NETDEBUG
            std::cout << thread_id << "couldnt load certificates" << std::endl;
#endif
            log_ssl_errors("couldnt load certificates from %s", certificate_path.c_str());
            //tidy up
            SSL_CTX_free(ctx);
            ctx = NULL;
            return -2;
        }
    } else if (!SSL_CTX_set_default_verify_paths(ctx)) //use default if no certPpath given
    {
#ifdef NETDEBUG
        std::cout << thread_id << "couldnt load certificates" << std::endl;
#endif
            log_ssl_errors("couldnt load default certificates for %s", hostname.c_str());
        //tidy up
        SSL_CTX_free(ctx);
        ctx = NULL;
        return -2;
    }

    // add validation params
    ERR_clear_error();
    X509_VERIFY_PARAM *x509_param = X509_VERIFY_PARAM_new();
    if (!x509_param) {
        log_ssl_errors("couldnt add validation params for %s", hostname.c_str());
        //X509_VERIFY_PARAM_free(x509_param);
            SSL_CTX_free(ctx);
            ctx = NULL;
        return -2;
    }

    ERR_clear_error();
    if (!X509_VERIFY_PARAM_set_flags(x509_param, X509_V_FLAG_TRUSTED_FIRST)) {
        log_ssl_errors("couldnt add validation params for %s", hostname.c_str());
        X509_VERIFY_PARAM_free(x509_param);
            SSL_CTX_free(ctx);
            ctx = NULL;
        return -2;
    }

    ERR_clear_error();
    if (!SSL_CTX_set1_param(ctx, x509_param)) {
        log_ssl_errors("couldnt add validation params for %s", hostname.c_str());
        X509_VERIFY_PARAM_free(x509_param);
            SSL_CTX_free(ctx);
            ctx = NULL;
        return -2;
    }

    X509_VERIFY_PARAM_free(x509_param);     // try not freeing this as SSL_CTX_free seems to be ring to free it

    //hand socket over to ssl lib
    ERR_clear_error();
    ssl = SSL_new(ctx);
    SSL_set_options(ssl, SSL_OP_ALL);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    SSL_set_connect_state(ssl);

    //fcntl(this->getFD() ,F_SETFL, O_NONBLOCK); // blocking mode used currently
    SSL_set_fd(ssl, this->getFD());
    SSL_set_tlsext_host_name(ssl, hostname.c_str());
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#else
  X509_VERIFY_PARAM_set1_host(SSL_get0_param(ssl),hostname.c_str(),0);
#endif

    //make io non blocking as select wont tell us if we can do a read without blocking
    //BIO_set_nbio(SSL_get_rbio(ssl),1l);  // blocking mode used currently
    //BIO_set_nbio(SSL_get_wbio(ssl),1l); // blocking mode used currently
    ERR_clear_error();
    int rc = SSL_connect(ssl);
    if (rc < 0) {
        log_ssl_errors("ssl_connect failed to %s", hostname.c_str());
#ifdef NETDEBUG
        std::cout << thread_id << "ssl_connect failed with error " << SSL_get_error(ssl, rc) << std::endl;
#endif
        // tidy up
        SSL_free(ssl);
        ssl = NULL;
        SSL_CTX_free(ctx);
        ctx = NULL;
        return -3;
    }

    //should be safer to do this last as nothing will ever try to use a ssl socket that isnt fully setup
    isssl = true;
    issslserver = false;
    return 0;
}