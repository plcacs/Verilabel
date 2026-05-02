static int ssl_verify_server_cert(Vio *vio, const char* server_hostname, const char **errptr)
{
  SSL *ssl;
  X509 *server_cert;
  X509_NAME *x509sn;
  int cn_pos;
  X509_NAME_ENTRY *cn_entry;
  ASN1_STRING *cn_asn1;
  const char *cn_str;
  DBUG_ENTER("ssl_verify_server_cert");
  DBUG_PRINT("enter", ("server_hostname: %s", server_hostname));

  if (!(ssl= (SSL*)vio->ssl_arg))
  {
    *errptr= "No SSL pointer found";
    DBUG_RETURN(1);
  }

  if (!server_hostname)
  {
    *errptr= "No server hostname supplied";
    DBUG_RETURN(1);
  }

  if (!(server_cert= SSL_get_peer_certificate(ssl)))
  {
    *errptr= "Could not get server certificate";
    DBUG_RETURN(1);
  }

  if (X509_V_OK != SSL_get_verify_result(ssl))
  {
    *errptr= "Failed to verify the server certificate";
    X509_free(server_cert);
    DBUG_RETURN(1);
  }
  /*
    We already know that the certificate exchanged was valid; the SSL library
    handled that. Now we need to verify that the contents of the certificate
    are what we expect.
  */

  x509sn= X509_get_subject_name(server_cert);

  if ((cn_pos= X509_NAME_get_index_by_NID(x509sn, NID_commonName, -1)) < 0)
    goto err;

  if (!(cn_entry= X509_NAME_get_entry(x509sn, cn_pos)))
    goto err;

  if (!(cn_asn1 = X509_NAME_ENTRY_get_data(cn_entry)))
    goto err;

  cn_str = (char *)ASN1_STRING_data(cn_asn1);

  /* Make sure there is no embedded \0 in the CN */
  if ((size_t)ASN1_STRING_length(cn_asn1) != strlen(cn_str))
    goto err;

  if (strcmp(cn_str, server_hostname))
    goto err;

  X509_free (server_cert);
  DBUG_RETURN(0);

err:
  X509_free(server_cert);
  *errptr= "SSL certificate validation failure";
  DBUG_RETURN(1);
}