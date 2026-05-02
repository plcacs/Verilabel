polarssl_connect_step1(struct connectdata *conn,
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

  /* PolarSSL only supports SSLv3 and TLSv1 */
  if(data->set.ssl.version == CURL_SSLVERSION_SSLv2) {
    failf(data, "PolarSSL does not support SSLv2");
    return CURLE_SSL_CONNECT_ERROR;
  }
  else if(data->set.ssl.version == CURL_SSLVERSION_SSLv3)
    sni = FALSE; /* SSLv3 has no SNI */

#ifdef THREADING_SUPPORT
  entropy_init_mutex(&entropy);

  if((ret = ctr_drbg_init(&connssl->ctr_drbg, entropy_func_mutex, &entropy,
                          NULL, 0)) != 0) {
#ifdef POLARSSL_ERROR_C
     error_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* POLARSSL_ERROR_C */
     failf(data, "Failed - PolarSSL: ctr_drbg_init returned (-0x%04X) %s\n",
                                                            -ret, errorbuf);
  }
#else
  entropy_init(&connssl->entropy);

  if((ret = ctr_drbg_init(&connssl->ctr_drbg, entropy_func, &connssl->entropy,
                          NULL, 0)) != 0) {
#ifdef POLARSSL_ERROR_C
     error_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* POLARSSL_ERROR_C */
     failf(data, "Failed - PolarSSL: ctr_drbg_init returned (-0x%04X) %s\n",
                                                            -ret, errorbuf);
  }
#endif /* THREADING_SUPPORT */

  /* Load the trusted CA */
  memset(&connssl->cacert, 0, sizeof(x509_crt));

  if(data->set.str[STRING_SSL_CAFILE]) {
    ret = x509_crt_parse_file(&connssl->cacert,
                              data->set.str[STRING_SSL_CAFILE]);

    if(ret<0) {
#ifdef POLARSSL_ERROR_C
      error_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* POLARSSL_ERROR_C */
      failf(data, "Error reading ca cert file %s - PolarSSL: (-0x%04X) %s",
            data->set.str[STRING_SSL_CAFILE], -ret, errorbuf);

      if(data->set.ssl.verifypeer)
        return CURLE_SSL_CACERT_BADFILE;
    }
  }

  if(data->set.str[STRING_SSL_CAPATH]) {
    ret = x509_crt_parse_path(&connssl->cacert,
                              data->set.str[STRING_SSL_CAPATH]);

    if(ret<0) {
#ifdef POLARSSL_ERROR_C
      error_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* POLARSSL_ERROR_C */
      failf(data, "Error reading ca cert path %s - PolarSSL: (-0x%04X) %s",
            data->set.str[STRING_SSL_CAPATH], -ret, errorbuf);

      if(data->set.ssl.verifypeer)
        return CURLE_SSL_CACERT_BADFILE;
    }
  }

  /* Load the client certificate */
  memset(&connssl->clicert, 0, sizeof(x509_crt));

  if(data->set.str[STRING_CERT]) {
    ret = x509_crt_parse_file(&connssl->clicert,
                              data->set.str[STRING_CERT]);

    if(ret) {
#ifdef POLARSSL_ERROR_C
      error_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* POLARSSL_ERROR_C */
      failf(data, "Error reading client cert file %s - PolarSSL: (-0x%04X) %s",
            data->set.str[STRING_CERT], -ret, errorbuf);

      return CURLE_SSL_CERTPROBLEM;
    }
  }

  /* Load the client private key */
  if(data->set.str[STRING_KEY]) {
    pk_context pk;
    pk_init(&pk);
    ret = pk_parse_keyfile(&pk, data->set.str[STRING_KEY],
                           data->set.str[STRING_KEY_PASSWD]);
    if(ret == 0 && !pk_can_do(&pk, POLARSSL_PK_RSA))
      ret = POLARSSL_ERR_PK_TYPE_MISMATCH;
    if(ret == 0)
      rsa_copy(&connssl->rsa, pk_rsa(pk));
    else
      rsa_free(&connssl->rsa);
    pk_free(&pk);

    if(ret) {
#ifdef POLARSSL_ERROR_C
      error_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* POLARSSL_ERROR_C */
      failf(data, "Error reading private key %s - PolarSSL: (-0x%04X) %s",
            data->set.str[STRING_KEY], -ret, errorbuf);

      return CURLE_SSL_CERTPROBLEM;
    }
  }

  /* Load the CRL */
  memset(&connssl->crl, 0, sizeof(x509_crl));

  if(data->set.str[STRING_SSL_CRLFILE]) {
    ret = x509_crl_parse_file(&connssl->crl,
                              data->set.str[STRING_SSL_CRLFILE]);

    if(ret) {
#ifdef POLARSSL_ERROR_C
      error_strerror(ret, errorbuf, sizeof(errorbuf));
#endif /* POLARSSL_ERROR_C */
      failf(data, "Error reading CRL file %s - PolarSSL: (-0x%04X) %s",
            data->set.str[STRING_SSL_CRLFILE], -ret, errorbuf);

      return CURLE_SSL_CRL_BADFILE;
    }
  }

  infof(data, "PolarSSL: Connecting to %s:%d\n",
        conn->host.name, conn->remote_port);

  if(ssl_init(&connssl->ssl)) {
    failf(data, "PolarSSL: ssl_init failed");
    return CURLE_SSL_CONNECT_ERROR;
  }

  switch(data->set.ssl.version) {
  default:
  case CURL_SSLVERSION_DEFAULT:
  case CURL_SSLVERSION_TLSv1:
    ssl_set_min_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_1);
    break;
  case CURL_SSLVERSION_SSLv3:
    ssl_set_min_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_0);
    ssl_set_max_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_0);
    infof(data, "PolarSSL: Forced min. SSL Version to be SSLv3\n");
    break;
  case CURL_SSLVERSION_TLSv1_0:
    ssl_set_min_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_1);
    ssl_set_max_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_1);
    infof(data, "PolarSSL: Forced min. SSL Version to be TLS 1.0\n");
    break;
  case CURL_SSLVERSION_TLSv1_1:
    ssl_set_min_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_2);
    ssl_set_max_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_2);
    infof(data, "PolarSSL: Forced min. SSL Version to be TLS 1.1\n");
    break;
  case CURL_SSLVERSION_TLSv1_2:
    ssl_set_min_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_3);
    ssl_set_max_version(&connssl->ssl, SSL_MAJOR_VERSION_3,
                        SSL_MINOR_VERSION_3);
    infof(data, "PolarSSL: Forced min. SSL Version to be TLS 1.2\n");
    break;
  }

  ssl_set_endpoint(&connssl->ssl, SSL_IS_CLIENT);
  ssl_set_authmode(&connssl->ssl, SSL_VERIFY_OPTIONAL);

  ssl_set_rng(&connssl->ssl, ctr_drbg_random,
              &connssl->ctr_drbg);
  ssl_set_bio(&connssl->ssl,
              net_recv, &conn->sock[sockindex],
              net_send, &conn->sock[sockindex]);

  ssl_set_ciphersuites(&connssl->ssl, ssl_list_ciphersuites());
  if(!Curl_ssl_getsessionid(conn, &old_session, NULL)) {
    ret = ssl_set_session(&connssl->ssl, old_session);
    if(ret) {
      failf(data, "ssl_set_session returned -0x%x", -ret);
      return CURLE_SSL_CONNECT_ERROR;
    }
    infof(data, "PolarSSL re-using session\n");
  }

  ssl_set_ca_chain(&connssl->ssl,
                   &connssl->cacert,
                   &connssl->crl,
                   conn->host.name);

  ssl_set_own_cert_rsa(&connssl->ssl,
                       &connssl->clicert, &connssl->rsa);

  if(!Curl_inet_pton(AF_INET, conn->host.name, &addr) &&
#ifdef ENABLE_IPV6
     !Curl_inet_pton(AF_INET6, conn->host.name, &addr) &&
#endif
     sni && ssl_set_hostname(&connssl->ssl, conn->host.name)) {
     infof(data, "WARNING: failed to configure "
                 "server name indication (SNI) TLS extension\n");
  }

#ifdef HAS_ALPN
  if(conn->bits.tls_enable_alpn) {
    static const char* protocols[3];
    int cur = 0;

#ifdef USE_NGHTTP2
    if(data->set.httpversion >= CURL_HTTP_VERSION_2) {
      protocols[cur++] = NGHTTP2_PROTO_VERSION_ID;
      infof(data, "ALPN, offering %s\n", NGHTTP2_PROTO_VERSION_ID);
    }
#endif

    protocols[cur++] = ALPN_HTTP_1_1;
    infof(data, "ALPN, offering %s\n", ALPN_HTTP_1_1);

    protocols[cur] = NULL;

    ssl_set_alpn_protocols(&connssl->ssl, protocols);
  }
#endif

#ifdef POLARSSL_DEBUG
  ssl_set_dbg(&connssl->ssl, polarssl_debug, data);
#endif

  connssl->connecting_state = ssl_connect_2;

  return CURLE_OK;
}