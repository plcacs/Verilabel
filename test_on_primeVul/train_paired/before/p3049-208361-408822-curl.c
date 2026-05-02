mbed_connect_step1(struct connectdata *conn,
                   int sockindex)
{
  struct SessionHandle *data = conn->data;
  struct ssl_connect_data* connssl = &conn->ssl[sockindex];

  bool sni = TRUE; /* default is SNI enabled */
  int ret = -1;
#ifdef ENABLE_IPV6
  struct in6_addr addr;
#else
  struct in_addr addr;
#endif
  void *old_session = NULL;
  char errorbuf[128];
  errorbuf[0]=0;

  /* mbedTLS only supports SSLv3 and TLSv1 */
  if(data->set.ssl.version == CURL_SSLVERSION_SSLv2) {
    failf(data, "mbedTLS does not support SSLv2");
    return CURLE_SSL_CONNECT_ERROR;
  }
  else if(data->set.ssl.version == CURL_SSLVERSION_SSLv3)
    sni = FALSE; /* SSLv3 has no SNI */

#ifdef THREADING_SUPPORT
  entropy_init_mutex(&entropy);
  mbedtls_ctr_drbg_init(&connssl->ctr_drbg);

  ret = mbedtls_ctr_drbg_seed(&connssl->ctr_drbg, entropy_func_mutex,
                              &entropy, NULL, 0);
  if(ret) {
#ifdef MBEDTLS_ERROR_C
    mbedtls_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* MBEDTLS_ERROR_C */
    failf(data, "Failed - mbedTLS: ctr_drbg_init returned (-0x%04X) %s\n",
          -ret, errorbuf);
  }
#else
  mbedtls_entropy_init(&connssl->entropy);
  mbedtls_ctr_drbg_init(&connssl->ctr_drbg);

  ret = mbedtls_ctr_drbg_seed(&connssl->ctr_drbg, mbedtls_entropy_func,
                              &connssl->entropy, NULL, 0);
  if(ret) {
#ifdef MBEDTLS_ERROR_C
    mbedtls_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* MBEDTLS_ERROR_C */
    failf(data, "Failed - mbedTLS: ctr_drbg_init returned (-0x%04X) %s\n",
          -ret, errorbuf);
  }
#endif /* THREADING_SUPPORT */

  /* Load the trusted CA */
  mbedtls_x509_crt_init(&connssl->cacert);

  if(data->set.str[STRING_SSL_CAFILE]) {
    ret = mbedtls_x509_crt_parse_file(&connssl->cacert,
                                      data->set.str[STRING_SSL_CAFILE]);

    if(ret<0) {
#ifdef MBEDTLS_ERROR_C
      mbedtls_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* MBEDTLS_ERROR_C */
      failf(data, "Error reading ca cert file %s - mbedTLS: (-0x%04X) %s",
            data->set.str[STRING_SSL_CAFILE], -ret, errorbuf);

      if(data->set.ssl.verifypeer)
        return CURLE_SSL_CACERT_BADFILE;
    }
  }

  if(data->set.str[STRING_SSL_CAPATH]) {
    ret = mbedtls_x509_crt_parse_path(&connssl->cacert,
                                      data->set.str[STRING_SSL_CAPATH]);

    if(ret<0) {
#ifdef MBEDTLS_ERROR_C
      mbedtls_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* MBEDTLS_ERROR_C */
      failf(data, "Error reading ca cert path %s - mbedTLS: (-0x%04X) %s",
            data->set.str[STRING_SSL_CAPATH], -ret, errorbuf);

      if(data->set.ssl.verifypeer)
        return CURLE_SSL_CACERT_BADFILE;
    }
  }

  /* Load the client certificate */
  mbedtls_x509_crt_init(&connssl->clicert);

  if(data->set.str[STRING_CERT]) {
    ret = mbedtls_x509_crt_parse_file(&connssl->clicert,
                                      data->set.str[STRING_CERT]);

    if(ret) {
#ifdef MBEDTLS_ERROR_C
      mbedtls_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* MBEDTLS_ERROR_C */
      failf(data, "Error reading client cert file %s - mbedTLS: (-0x%04X) %s",
            data->set.str[STRING_CERT], -ret, errorbuf);

      return CURLE_SSL_CERTPROBLEM;
    }
  }

  /* Load the client private key */
  mbedtls_pk_init(&connssl->pk);

  if(data->set.str[STRING_KEY]) {
    ret = mbedtls_pk_parse_keyfile(&connssl->pk, data->set.str[STRING_KEY],
                                   data->set.str[STRING_KEY_PASSWD]);
    if(ret == 0 && !mbedtls_pk_can_do(&connssl->pk, MBEDTLS_PK_RSA))
      ret = MBEDTLS_ERR_PK_TYPE_MISMATCH;

    if(ret) {
#ifdef MBEDTLS_ERROR_C
      mbedtls_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* MBEDTLS_ERROR_C */
      failf(data, "Error reading private key %s - mbedTLS: (-0x%04X) %s",
            data->set.str[STRING_KEY], -ret, errorbuf);

      return CURLE_SSL_CERTPROBLEM;
    }
  }

  /* Load the CRL */
  mbedtls_x509_crl_init(&connssl->crl);

  if(data->set.str[STRING_SSL_CRLFILE]) {
    ret = mbedtls_x509_crl_parse_file(&connssl->crl,
                                      data->set.str[STRING_SSL_CRLFILE]);

    if(ret) {
#ifdef MBEDTLS_ERROR_C
      mbedtls_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* MBEDTLS_ERROR_C */
      failf(data, "Error reading CRL file %s - mbedTLS: (-0x%04X) %s",
            data->set.str[STRING_SSL_CRLFILE], -ret, errorbuf);

      return CURLE_SSL_CRL_BADFILE;
    }
  }

  infof(data, "mbedTLS: Connecting to %s:%d\n",
        conn->host.name, conn->remote_port);

  mbedtls_ssl_config_init(&connssl->config);

  mbedtls_ssl_init(&connssl->ssl);
  if(mbedtls_ssl_setup(&connssl->ssl, &connssl->config)) {
    failf(data, "mbedTLS: ssl_init failed");
    return CURLE_SSL_CONNECT_ERROR;
  }
  ret = mbedtls_ssl_config_defaults(&connssl->config,
                                    MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
  if(ret) {
    failf(data, "mbedTLS: ssl_config failed");
    return CURLE_SSL_CONNECT_ERROR;
  }

  /* new profile with RSA min key len = 1024 ... */
  mbedtls_ssl_conf_cert_profile(&connssl->config,
                                &mbedtls_x509_crt_profile_fr);

  switch(data->set.ssl.version) {
  case CURL_SSLVERSION_DEFAULT:
  case CURL_SSLVERSION_TLSv1:
    mbedtls_ssl_conf_min_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_1);
    infof(data, "mbedTLS: Set min SSL version to TLS 1.0\n");
    break;
  case CURL_SSLVERSION_SSLv3:
    mbedtls_ssl_conf_min_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_0);
    mbedtls_ssl_conf_max_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_0);
    infof(data, "mbedTLS: Set SSL version to SSLv3\n");
    break;
  case CURL_SSLVERSION_TLSv1_0:
    mbedtls_ssl_conf_min_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_1);
    mbedtls_ssl_conf_max_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_1);
    infof(data, "mbedTLS: Set SSL version to TLS 1.0\n");
    break;
  case CURL_SSLVERSION_TLSv1_1:
    mbedtls_ssl_conf_min_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_2);
    mbedtls_ssl_conf_max_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_2);
    infof(data, "mbedTLS: Set SSL version to TLS 1.1\n");
    break;
  case CURL_SSLVERSION_TLSv1_2:
    mbedtls_ssl_conf_min_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&connssl->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_3);
    infof(data, "mbedTLS: Set SSL version to TLS 1.2\n");
    break;
  default:
    failf(data, "mbedTLS: Unsupported SSL protocol version");
    return CURLE_SSL_CONNECT_ERROR;
  }

  mbedtls_ssl_conf_authmode(&connssl->config, MBEDTLS_SSL_VERIFY_OPTIONAL);

  mbedtls_ssl_conf_rng(&connssl->config, mbedtls_ctr_drbg_random,
                       &connssl->ctr_drbg);
  mbedtls_ssl_set_bio(&connssl->ssl, &conn->sock[sockindex],
                      mbedtls_net_send,
                      mbedtls_net_recv,
                      NULL /*  rev_timeout() */);

  mbedtls_ssl_conf_ciphersuites(&connssl->config,
                                mbedtls_ssl_list_ciphersuites());
  if(!Curl_ssl_getsessionid(conn, &old_session, NULL)) {
    ret = mbedtls_ssl_set_session(&connssl->ssl, old_session);
    if(ret) {
      failf(data, "mbedtls_ssl_set_session returned -0x%x", -ret);
      return CURLE_SSL_CONNECT_ERROR;
    }
    infof(data, "mbedTLS re-using session\n");
  }

  mbedtls_ssl_conf_ca_chain(&connssl->config,
                            &connssl->cacert,
                            &connssl->crl);

  if(data->set.str[STRING_KEY]) {
    mbedtls_ssl_conf_own_cert(&connssl->config,
                              &connssl->clicert, &connssl->pk);
  }
  if(!Curl_inet_pton(AF_INET, conn->host.name, &addr) &&
#ifdef ENABLE_IPV6
     !Curl_inet_pton(AF_INET6, conn->host.name, &addr) &&
#endif
     sni && mbedtls_ssl_set_hostname(&connssl->ssl, conn->host.name)) {
    infof(data, "WARNING: failed to configure "
          "server name indication (SNI) TLS extension\n");
  }

#ifdef HAS_ALPN
  if(conn->bits.tls_enable_alpn) {
    const char **p = &connssl->protocols[0];
#ifdef USE_NGHTTP2
    if(data->set.httpversion >= CURL_HTTP_VERSION_2)
      *p++ = NGHTTP2_PROTO_VERSION_ID;
#endif
    *p++ = ALPN_HTTP_1_1;
    *p = NULL;
    /* this function doesn't clone the protocols array, which is why we need
       to keep it around */
    if(mbedtls_ssl_conf_alpn_protocols(&connssl->config,
                                       &connssl->protocols[0])) {
      failf(data, "Failed setting ALPN protocols");
      return CURLE_SSL_CONNECT_ERROR;
    }
    for(p = &connssl->protocols[0]; *p; ++p)
      infof(data, "ALPN, offering %s\n", *p);
  }
#endif

#ifdef MBEDTLS_DEBUG
  mbedtls_ssl_conf_dbg(&connssl->config, mbedtls_debug, data);
#endif

  connssl->connecting_state = ssl_connect_2;

  return CURLE_OK;
}