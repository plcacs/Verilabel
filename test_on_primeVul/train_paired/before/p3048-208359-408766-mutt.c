static void cmd_parse_status (IMAP_DATA* idata, char* s)
{
  char* mailbox;
  char* value;
  BUFFY* inc;
  IMAP_MBOX mx;
  unsigned long ulcount;
  unsigned int count;
  IMAP_STATUS *status;
  unsigned int olduv, oldun;
  unsigned int litlen;
  short new = 0;
  short new_msg_count = 0;

  mailbox = imap_next_word (s);

  /* We need a real tokenizer. */
  if (!imap_get_literal_count (mailbox, &litlen))
  {
    if (imap_cmd_step (idata) != IMAP_CMD_CONTINUE)
    {
      idata->status = IMAP_FATAL;
      return;
    }
    mailbox = idata->buf;
    s = mailbox + litlen;
    *s = '\0';
    s++;
    SKIPWS(s);
  }
  else
  {
    s = imap_next_word (mailbox);
    *(s - 1) = '\0';
    imap_unmunge_mbox_name (idata, mailbox);
  }

  status = imap_mboxcache_get (idata, mailbox, 1);
  olduv = status->uidvalidity;
  oldun = status->uidnext;

  if (*s++ != '(')
  {
    dprint (1, (debugfile, "Error parsing STATUS\n"));
    return;
  }
  while (*s && *s != ')')
  {
    value = imap_next_word (s);

    errno = 0;
    ulcount = strtoul (value, &value, 10);
    if ((errno == ERANGE && ulcount == ULONG_MAX) ||
        ((unsigned int) ulcount != ulcount))
    {
      dprint (1, (debugfile, "Error parsing STATUS number\n"));
      return;
    }
    count = (unsigned int) ulcount;

    if (!ascii_strncmp ("MESSAGES", s, 8))
    {
      status->messages = count;
      new_msg_count = 1;
    }
    else if (!ascii_strncmp ("RECENT", s, 6))
      status->recent = count;
    else if (!ascii_strncmp ("UIDNEXT", s, 7))
      status->uidnext = count;
    else if (!ascii_strncmp ("UIDVALIDITY", s, 11))
      status->uidvalidity = count;
    else if (!ascii_strncmp ("UNSEEN", s, 6))
      status->unseen = count;

    s = value;
    if (*s && *s != ')')
      s = imap_next_word (s);
  }
  dprint (3, (debugfile, "%s (UIDVALIDITY: %u, UIDNEXT: %u) %d messages, %d recent, %d unseen\n",
              status->name, status->uidvalidity, status->uidnext,
              status->messages, status->recent, status->unseen));

  /* caller is prepared to handle the result herself */
  if (idata->cmddata && idata->cmdtype == IMAP_CT_STATUS)
  {
    memcpy (idata->cmddata, status, sizeof (IMAP_STATUS));
    return;
  }

  dprint (3, (debugfile, "Running default STATUS handler\n"));

  /* should perhaps move this code back to imap_buffy_check */
  for (inc = Incoming; inc; inc = inc->next)
  {
    if (inc->magic != MUTT_IMAP)
      continue;
    
    if (imap_parse_path (inc->path, &mx) < 0)
    {
      dprint (1, (debugfile, "Error parsing mailbox %s, skipping\n", inc->path));
      continue;
    }
    /* dprint (2, (debugfile, "Buffy entry: [%s] mbox: [%s]\n", inc->path, NONULL(mx.mbox))); */
    
    if (imap_account_match (&idata->conn->account, &mx.account))
    {
      if (mx.mbox)
      {
	value = safe_strdup (mx.mbox);
	imap_fix_path (idata, mx.mbox, value, mutt_strlen (value) + 1);
	FREE (&mx.mbox);
      }
      else
	value = safe_strdup ("INBOX");

      if (value && !imap_mxcmp (mailbox, value))
      {
        dprint (3, (debugfile, "Found %s in buffy list (OV: %u ON: %u U: %d)\n",
                    mailbox, olduv, oldun, status->unseen));
        
	if (option(OPTMAILCHECKRECENT))
	{
	  if (olduv && olduv == status->uidvalidity)
	  {
	    if (oldun < status->uidnext)
	      new = (status->unseen > 0);
	  }
	  else if (!olduv && !oldun)
	    /* first check per session, use recent. might need a flag for this. */
	    new = (status->recent > 0);
	  else
	    new = (status->unseen > 0);
	}
	else
          new = (status->unseen > 0);

#ifdef USE_SIDEBAR
        if ((inc->new != new) ||
            (inc->msg_count != status->messages) ||
            (inc->msg_unread != status->unseen))
          mutt_set_current_menu_redraw (REDRAW_SIDEBAR);
#endif
        inc->new = new;
        if (new_msg_count)
          inc->msg_count = status->messages;
        inc->msg_unread = status->unseen;

	if (inc->new)
	  /* force back to keep detecting new mail until the mailbox is
	     opened */
	  status->uidnext = oldun;

        FREE (&value);
        return;
      }

      FREE (&value);
    }

    FREE (&mx.mbox);
  }
}