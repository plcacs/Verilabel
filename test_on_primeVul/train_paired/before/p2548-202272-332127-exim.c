void dkim_exim_verify_finish(void) {
  pdkim_signature *sig = NULL;
  int dkim_signers_size = 0;
  int dkim_signers_ptr = 0;
  dkim_signers = NULL;

  /* Delete eventual previous signature chain */
  dkim_signatures = NULL;

  /* If we have arrived here with dkim_collect_input == FALSE, it
     means there was a processing error somewhere along the way.
     Log the incident and disable futher verification. */
  if (!dkim_collect_input) {
    log_write(0, LOG_MAIN, "DKIM: Error while running this message through validation, disabling signature verification.");
    dkim_disable_verify = TRUE;
    return;
  }
  dkim_collect_input = FALSE;

  /* Finish DKIM operation and fetch link to signatures chain */
  if (pdkim_feed_finish(dkim_verify_ctx,&dkim_signatures) != PDKIM_OK) return;

  sig = dkim_signatures;
  while (sig != NULL) {
    int size = 0;
    int ptr = 0;
    /* Log a line for each signature */
    uschar *logmsg = string_append(NULL, &size, &ptr, 5,

      string_sprintf( "DKIM: d=%s s=%s c=%s/%s a=%s ",
                      sig->domain,
                      sig->selector,
                      (sig->canon_headers == PDKIM_CANON_SIMPLE)?"simple":"relaxed",
                      (sig->canon_body    == PDKIM_CANON_SIMPLE)?"simple":"relaxed",
                      (sig->algo          == PDKIM_ALGO_RSA_SHA256)?"rsa-sha256":"rsa-sha1"
                    ),
      ((sig->identity != NULL)?
        string_sprintf("i=%s ", sig->identity)
        :
        US""
      ),
      ((sig->created > 0)?
        string_sprintf("t=%lu ", sig->created)
        :
        US""
      ),
      ((sig->expires > 0)?
        string_sprintf("x=%lu ", sig->expires)
        :
        US""
      ),
      ((sig->bodylength > -1)?
        string_sprintf("l=%lu ", sig->bodylength)
        :
        US""
      )
    );

    switch(sig->verify_status) {
      case PDKIM_VERIFY_NONE:
        logmsg = string_append(logmsg, &size, &ptr, 1, "[not verified]");
      break;
      case PDKIM_VERIFY_INVALID:
        logmsg = string_append(logmsg, &size, &ptr, 1, "[invalid - ");
        switch (sig->verify_ext_status) {
          case PDKIM_VERIFY_INVALID_PUBKEY_UNAVAILABLE:
            logmsg = string_append(logmsg, &size, &ptr, 1, "public key record (currently?) unavailable]");
          break;
          case PDKIM_VERIFY_INVALID_BUFFER_SIZE:
            logmsg = string_append(logmsg, &size, &ptr, 1, "overlong public key record]");
          break;
          case PDKIM_VERIFY_INVALID_PUBKEY_PARSING:
            logmsg = string_append(logmsg, &size, &ptr, 1, "syntax error in public key record]");
          break;
          default:
            logmsg = string_append(logmsg, &size, &ptr, 1, "unspecified problem]");
        }
      break;
      case PDKIM_VERIFY_FAIL:
        logmsg = string_append(logmsg, &size, &ptr, 1, "[verification failed - ");
        switch (sig->verify_ext_status) {
          case PDKIM_VERIFY_FAIL_BODY:
            logmsg = string_append(logmsg, &size, &ptr, 1, "body hash mismatch (body probably modified in transit)]");
          break;
          case PDKIM_VERIFY_FAIL_MESSAGE:
            logmsg = string_append(logmsg, &size, &ptr, 1, "signature did not verify (headers probably modified in transit)]");
          break;
          default:
            logmsg = string_append(logmsg, &size, &ptr, 1, "unspecified reason]");
        }
      break;
      case PDKIM_VERIFY_PASS:
        logmsg = string_append(logmsg, &size, &ptr, 1, "[verification succeeded]");
      break;
    }

    logmsg[ptr] = '\0';
    log_write(0, LOG_MAIN, (char *)logmsg);

    /* Build a colon-separated list of signing domains (and identities, if present) in dkim_signers */
    dkim_signers = string_append(dkim_signers,
                                 &dkim_signers_size,
                                 &dkim_signers_ptr,
                                 2,
                                 sig->domain,
                                 ":"
                                );

    if (sig->identity != NULL) {
      dkim_signers = string_append(dkim_signers,
                                   &dkim_signers_size,
                                   &dkim_signers_ptr,
                                   2,
                                   sig->identity,
                                   ":"
                                  );
    }

    /* Process next signature */
    sig = sig->next;
  }

  /* NULL-terminate and chop the last colon from the domain list */
  if (dkim_signers != NULL) {
    dkim_signers[dkim_signers_ptr] = '\0';
    if (Ustrlen(dkim_signers) > 0)
      dkim_signers[Ustrlen(dkim_signers)-1] = '\0';
  }
}