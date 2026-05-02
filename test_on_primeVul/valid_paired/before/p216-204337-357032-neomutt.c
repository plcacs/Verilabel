int imap_open_connection(struct ImapAccountData *adata)
{
  if (mutt_socket_open(adata->conn) < 0)
    return -1;

  adata->state = IMAP_CONNECTED;

  if (imap_cmd_step(adata) != IMAP_RES_OK)
  {
    imap_close_connection(adata);
    return -1;
  }

  if (mutt_istr_startswith(adata->buf, "* OK"))
  {
    if (!mutt_istr_startswith(adata->buf, "* OK [CAPABILITY") && check_capabilities(adata))
    {
      goto bail;
    }
#ifdef USE_SSL
    /* Attempt STARTTLS if available and desired. */
    if ((adata->conn->ssf == 0) && (C_SslForceTls || (adata->capabilities & IMAP_CAP_STARTTLS)))
    {
      enum QuadOption ans;

      if (C_SslForceTls)
        ans = MUTT_YES;
      else if ((ans = query_quadoption(C_SslStarttls,
                                       _("Secure connection with TLS?"))) == MUTT_ABORT)
      {
        goto err_close_conn;
      }
      if (ans == MUTT_YES)
      {
        enum ImapExecResult rc = imap_exec(adata, "STARTTLS", IMAP_CMD_SINGLE);
        // Clear any data after the STARTTLS acknowledgement
        mutt_socket_empty(adata->conn);

        if (rc == IMAP_EXEC_FATAL)
          goto bail;
        if (rc != IMAP_EXEC_ERROR)
        {
          if (mutt_ssl_starttls(adata->conn))
          {
            mutt_error(_("Could not negotiate TLS connection"));
            goto err_close_conn;
          }
          else
          {
            /* RFC2595 demands we recheck CAPABILITY after TLS completes. */
            if (imap_exec(adata, "CAPABILITY", IMAP_CMD_NO_FLAGS))
              goto bail;
          }
        }
      }
    }

    if (C_SslForceTls && (adata->conn->ssf == 0))
    {
      mutt_error(_("Encrypted connection unavailable"));
      goto err_close_conn;
    }
#endif
  }
  else if (mutt_istr_startswith(adata->buf, "* PREAUTH"))
  {
#ifdef USE_SSL
    /* Unless using a secure $tunnel, an unencrypted PREAUTH response may be a
     * MITM attack.  The only way to stop "STARTTLS" MITM attacks is via
     * $ssl_force_tls: an attacker can easily spoof "* OK" and strip the
     * STARTTLS capability.  So consult $ssl_force_tls, not $ssl_starttls, to
     * decide whether to abort. Note that if using $tunnel and
     * $tunnel_is_secure, adata->conn->ssf will be set to 1. */
    if ((adata->conn->ssf == 0) && C_SslForceTls)
    {
      mutt_error(_("Encrypted connection unavailable"));
      goto err_close_conn;
    }
#endif

    adata->state = IMAP_AUTHENTICATED;
    if (check_capabilities(adata) != 0)
      goto bail;
    FREE(&adata->capstr);
  }
  else
  {
    imap_error("imap_open_connection()", adata->buf);
    goto bail;
  }

  return 0;

#ifdef USE_SSL
err_close_conn:
  imap_close_connection(adata);
#endif
bail:
  FREE(&adata->capstr);
  return -1;
}