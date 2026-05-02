static int cmd_handle_untagged (IMAP_DATA* idata)
{
  char* s;
  char* pn;
  unsigned int count;

  s = imap_next_word (idata->buf);
  pn = imap_next_word (s);

  if ((idata->state >= IMAP_SELECTED) && isdigit ((unsigned char) *s))
  {
    pn = s;
    s = imap_next_word (s);

    /* EXISTS and EXPUNGE are always related to the SELECTED mailbox for the
     * connection, so update that one.
     */
    if (ascii_strncasecmp ("EXISTS", s, 6) == 0)
    {
      dprint (2, (debugfile, "Handling EXISTS\n"));

      /* new mail arrived */
      mutt_atoui (pn, &count);

      if ( !(idata->reopen & IMAP_EXPUNGE_PENDING) &&
	   count < idata->max_msn)
      {
        /* Notes 6.0.3 has a tendency to report fewer messages exist than
         * it should. */
	dprint (1, (debugfile, "Message count is out of sync"));
	return 0;
      }
      /* at least the InterChange server sends EXISTS messages freely,
       * even when there is no new mail */
      else if (count == idata->max_msn)
	dprint (3, (debugfile,
          "cmd_handle_untagged: superfluous EXISTS message.\n"));
      else
      {
	if (!(idata->reopen & IMAP_EXPUNGE_PENDING))
        {
          dprint (2, (debugfile,
            "cmd_handle_untagged: New mail in %s - %d messages total.\n",
            idata->mailbox, count));
	  idata->reopen |= IMAP_NEWMAIL_PENDING;
        }
	idata->newMailCount = count;
      }
    }
    /* pn vs. s: need initial seqno */
    else if (ascii_strncasecmp ("EXPUNGE", s, 7) == 0)
      cmd_parse_expunge (idata, pn);
    else if (ascii_strncasecmp ("FETCH", s, 5) == 0)
      cmd_parse_fetch (idata, pn);
  }
  else if (ascii_strncasecmp ("CAPABILITY", s, 10) == 0)
    cmd_parse_capability (idata, s);
  else if (!ascii_strncasecmp ("OK [CAPABILITY", s, 14))
    cmd_parse_capability (idata, pn);
  else if (!ascii_strncasecmp ("OK [CAPABILITY", pn, 14))
    cmd_parse_capability (idata, imap_next_word (pn));
  else if (ascii_strncasecmp ("LIST", s, 4) == 0)
    cmd_parse_list (idata, s);
  else if (ascii_strncasecmp ("LSUB", s, 4) == 0)
    cmd_parse_lsub (idata, s);
  else if (ascii_strncasecmp ("MYRIGHTS", s, 8) == 0)
    cmd_parse_myrights (idata, s);
  else if (ascii_strncasecmp ("SEARCH", s, 6) == 0)
    cmd_parse_search (idata, s);
  else if (ascii_strncasecmp ("STATUS", s, 6) == 0)
    cmd_parse_status (idata, s);
  else if (ascii_strncasecmp ("ENABLED", s, 7) == 0)
    cmd_parse_enabled (idata, s);
  else if (ascii_strncasecmp ("BYE", s, 3) == 0)
  {
    dprint (2, (debugfile, "Handling BYE\n"));

    /* check if we're logging out */
    if (idata->status == IMAP_BYE)
      return 0;

    /* server shut down our connection */
    s += 3;
    SKIPWS (s);
    mutt_error ("%s", s);
    mutt_sleep (2);
    cmd_handle_fatal (idata);

    return -1;
  }
  else if (option (OPTIMAPSERVERNOISE) && (ascii_strncasecmp ("NO", s, 2) == 0))
  {
    dprint (2, (debugfile, "Handling untagged NO\n"));

    /* Display the warning message from the server */
    mutt_error ("%s", s+3);
    mutt_sleep (2);
  }

  return 0;
}