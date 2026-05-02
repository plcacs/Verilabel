static int ssl_verify_server_cert(Vio *vio, const char* server_hostname, const char **errptr)
{
  SSL *ssl;
  X509 *server_cert;
  char *cp1, *cp2;
  char *buf;
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

  buf= X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0);
  X509_free (server_cert);

  if (!buf)
  {
    *errptr= "Out of memory";
    DBUG_RETURN(1);
  }

  DBUG_PRINT("info", ("hostname in cert: %s", buf));
  cp1= strstr(buf, "/CN=");
  if (cp1)
  {
    cp1+= 4; /* Skip the "/CN=" that we found */
    /* Search for next / which might be the delimiter for email */
    cp2= strchr(cp1, '/');
    if (cp2)
      *cp2= '\0';
    DBUG_PRINT("info", ("Server hostname in cert: %s", cp1));
    if (!strcmp(cp1, server_hostname))
    {
      free(buf);
      /* Success */
      DBUG_RETURN(0);
    }
  }
  *errptr= "SSL certificate validation failure";
  free(buf);
  DBUG_RETURN(1);
}