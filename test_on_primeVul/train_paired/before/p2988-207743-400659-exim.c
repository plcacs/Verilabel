deliver_message(uschar *id, BOOL forced, BOOL give_up)
{
int i, rc;
int final_yield = DELIVER_ATTEMPTED_NORMAL;
time_t now = time(NULL);
address_item *addr_last = NULL;
uschar *filter_message = NULL;
int process_recipients = RECIP_ACCEPT;
open_db dbblock;
open_db *dbm_file;
extern int acl_where;

uschar *info = queue_run_pid == (pid_t)0
  ? string_sprintf("delivering %s", id)
  : string_sprintf("delivering %s (queue run pid %d)", id, queue_run_pid);

/* If the D_process_info bit is on, set_process_info() will output debugging
information. If not, we want to show this initial information if D_deliver or
D_queue_run is set or in verbose mode. */

set_process_info("%s", info);

if (  !(debug_selector & D_process_info)
   && (debug_selector & (D_deliver|D_queue_run|D_v))
   )
  debug_printf("%s\n", info);

/* Ensure that we catch any subprocesses that are created. Although Exim
sets SIG_DFL as its initial default, some routes through the code end up
here with it set to SIG_IGN - cases where a non-synchronous delivery process
has been forked, but no re-exec has been done. We use sigaction rather than
plain signal() on those OS where SA_NOCLDWAIT exists, because we want to be
sure it is turned off. (There was a problem on AIX with this.) */

#ifdef SA_NOCLDWAIT
  {
  struct sigaction act;
  act.sa_handler = SIG_DFL;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = 0;
  sigaction(SIGCHLD, &act, NULL);
  }
#else
signal(SIGCHLD, SIG_DFL);
#endif

/* Make the forcing flag available for routers and transports, set up the
global message id field, and initialize the count for returned files and the
message size. This use of strcpy() is OK because the length id is checked when
it is obtained from a command line (the -M or -q options), and otherwise it is
known to be a valid message id. */

if (id != message_id)
  Ustrcpy(message_id, id);
deliver_force = forced;
return_count = 0;
message_size = 0;

/* Initialize some flags */

update_spool = FALSE;
remove_journal = TRUE;

/* Set a known context for any ACLs we call via expansions */
acl_where = ACL_WHERE_DELIVERY;

/* Reset the random number generator, so that if several delivery processes are
started from a queue runner that has already used random numbers (for sorting),
they don't all get the same sequence. */

random_seed = 0;

/* Open and lock the message's data file. Exim locks on this one because the
header file may get replaced as it is re-written during the delivery process.
Any failures cause messages to be written to the log, except for missing files
while queue running - another process probably completed delivery. As part of
opening the data file, message_subdir gets set. */

if ((deliver_datafile = spool_open_datafile(id)) < 0)
  return continue_closedown();  /* yields DELIVER_NOT_ATTEMPTED */

/* The value of message_size at this point has been set to the data length,
plus one for the blank line that notionally precedes the data. */

/* Now read the contents of the header file, which will set up the headers in
store, and also the list of recipients and the tree of non-recipients and
assorted flags. It updates message_size. If there is a reading or format error,
give up; if the message has been around for sufficiently long, remove it. */

  {
  uschar * spoolname = string_sprintf("%s-H", id);
  if ((rc = spool_read_header(spoolname, TRUE, TRUE)) != spool_read_OK)
    {
    if (errno == ERRNO_SPOOLFORMAT)
      {
      struct stat statbuf;
      if (Ustat(spool_fname(US"input", message_subdir, spoolname, US""),
		&statbuf) == 0)
	log_write(0, LOG_MAIN, "Format error in spool file %s: "
	  "size=" OFF_T_FMT, spoolname, statbuf.st_size);
      else
	log_write(0, LOG_MAIN, "Format error in spool file %s", spoolname);
      }
    else
      log_write(0, LOG_MAIN, "Error reading spool file %s: %s", spoolname,
	strerror(errno));

    /* If we managed to read the envelope data, received_time contains the
    time the message was received. Otherwise, we can calculate it from the
    message id. */

    if (rc != spool_read_hdrerror)
      {
      received_time.tv_sec = received_time.tv_usec = 0;
      /*XXX subsec precision?*/
      for (i = 0; i < 6; i++)
	received_time.tv_sec = received_time.tv_sec * BASE_62 + tab62[id[i] - '0'];
      }

    /* If we've had this malformed message too long, sling it. */

    if (now - received_time.tv_sec > keep_malformed)
      {
      Uunlink(spool_fname(US"msglog", message_subdir, id, US""));
      Uunlink(spool_fname(US"input", message_subdir, id, US"-D"));
      Uunlink(spool_fname(US"input", message_subdir, id, US"-H"));
      Uunlink(spool_fname(US"input", message_subdir, id, US"-J"));
      log_write(0, LOG_MAIN, "Message removed because older than %s",
	readconf_printtime(keep_malformed));
      }

    (void)close(deliver_datafile);
    deliver_datafile = -1;
    return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
    }
  }

/* The spool header file has been read. Look to see if there is an existing
journal file for this message. If there is, it means that a previous delivery
attempt crashed (program or host) before it could update the spool header file.
Read the list of delivered addresses from the journal and add them to the
nonrecipients tree. Then update the spool file. We can leave the journal in
existence, as it will get further successful deliveries added to it in this
run, and it will be deleted if this function gets to its end successfully.
Otherwise it might be needed again. */

  {
  uschar * fname = spool_fname(US"input", message_subdir, id, US"-J");
  FILE * jread;

  if (  (journal_fd = Uopen(fname, O_RDWR|O_APPEND
#ifdef O_CLOEXEC
				    | O_CLOEXEC
#endif
#ifdef O_NOFOLLOW
				    | O_NOFOLLOW
#endif
	, SPOOL_MODE)) >= 0
     && lseek(journal_fd, 0, SEEK_SET) == 0
     && (jread = fdopen(journal_fd, "rb"))
     )
    {
    while (Ufgets(big_buffer, big_buffer_size, jread))
      {
      int n = Ustrlen(big_buffer);
      big_buffer[n-1] = 0;
      tree_add_nonrecipient(big_buffer);
      DEBUG(D_deliver) debug_printf("Previously delivered address %s taken from "
	"journal file\n", big_buffer);
      }
    rewind(jread);
    if ((journal_fd = dup(fileno(jread))) < 0)
      journal_fd = fileno(jread);
    else
      (void) fclose(jread);	/* Try to not leak the FILE resource */

    /* Panic-dies on error */
    (void)spool_write_header(message_id, SW_DELIVERING, NULL);
    }
  else if (errno != ENOENT)
    {
    log_write(0, LOG_MAIN|LOG_PANIC, "attempt to open journal for reading gave: "
      "%s", strerror(errno));
    return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
    }

  /* A null recipients list indicates some kind of disaster. */

  if (!recipients_list)
    {
    (void)close(deliver_datafile);
    deliver_datafile = -1;
    log_write(0, LOG_MAIN, "Spool error: no recipients for %s", fname);
    return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
    }
  }


/* Handle a message that is frozen. There are a number of different things that
can happen, but in the default situation, unless forced, no delivery is
attempted. */

if (deliver_freeze)
  {
#ifdef SUPPORT_MOVE_FROZEN_MESSAGES
  /* Moving to another directory removes the message from Exim's view. Other
  tools must be used to deal with it. Logging of this action happens in
  spool_move_message() and its subfunctions. */

  if (  move_frozen_messages
     && spool_move_message(id, message_subdir, US"", US"F")
     )
    return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
#endif

  /* For all frozen messages (bounces or not), timeout_frozen_after sets the
  maximum time to keep messages that are frozen. Thaw if we reach it, with a
  flag causing all recipients to be failed. The time is the age of the
  message, not the time since freezing. */

  if (timeout_frozen_after > 0 && message_age >= timeout_frozen_after)
    {
    log_write(0, LOG_MAIN, "cancelled by timeout_frozen_after");
    process_recipients = RECIP_FAIL_TIMEOUT;
    }

  /* For bounce messages (and others with no sender), thaw if the error message
  ignore timer is exceeded. The message will be discarded if this delivery
  fails. */

  else if (!*sender_address && message_age >= ignore_bounce_errors_after)
    log_write(0, LOG_MAIN, "Unfrozen by errmsg timer");

  /* If this is a bounce message, or there's no auto thaw, or we haven't
  reached the auto thaw time yet, and this delivery is not forced by an admin
  user, do not attempt delivery of this message. Note that forced is set for
  continuing messages down the same channel, in order to skip load checking and
  ignore hold domains, but we don't want unfreezing in that case. */

  else
    {
    if (  (  sender_address[0] == 0
	  || auto_thaw <= 0
	  || now <= deliver_frozen_at + auto_thaw
          )
       && (  !forced || !deliver_force_thaw
	  || !admin_user || continue_hostname
       )  )
      {
      (void)close(deliver_datafile);
      deliver_datafile = -1;
      log_write(L_skip_delivery, LOG_MAIN, "Message is frozen");
      return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
      }

    /* If delivery was forced (by an admin user), assume a manual thaw.
    Otherwise it's an auto thaw. */

    if (forced)
      {
      deliver_manual_thaw = TRUE;
      log_write(0, LOG_MAIN, "Unfrozen by forced delivery");
      }
    else log_write(0, LOG_MAIN, "Unfrozen by auto-thaw");
    }

  /* We get here if any of the rules for unfreezing have triggered. */

  deliver_freeze = FALSE;
  update_spool = TRUE;
  }


/* Open the message log file if we are using them. This records details of
deliveries, deferments, and failures for the benefit of the mail administrator.
The log is not used by exim itself to track the progress of a message; that is
done by rewriting the header spool file. */

if (message_logs)
  {
  uschar * fname = spool_fname(US"msglog", message_subdir, id, US"");
  uschar * error;
  int fd;

  if ((fd = open_msglog_file(fname, SPOOL_MODE, &error)) < 0)
    {
    log_write(0, LOG_MAIN|LOG_PANIC, "Couldn't %s message log %s: %s", error,
      fname, strerror(errno));
    return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
    }

  /* Make a C stream out of it. */

  if (!(message_log = fdopen(fd, "a")))
    {
    log_write(0, LOG_MAIN|LOG_PANIC, "Couldn't fdopen message log %s: %s",
      fname, strerror(errno));
    return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
    }
  }


/* If asked to give up on a message, log who did it, and set the action for all
the addresses. */

if (give_up)
  {
  struct passwd *pw = getpwuid(real_uid);
  log_write(0, LOG_MAIN, "cancelled by %s",
      pw ? US pw->pw_name : string_sprintf("uid %ld", (long int)real_uid));
  process_recipients = RECIP_FAIL;
  }

/* Otherwise, if there are too many Received: headers, fail all recipients. */

else if (received_count > received_headers_max)
  process_recipients = RECIP_FAIL_LOOP;

/* Otherwise, if a system-wide, address-independent message filter is
specified, run it now, except in the case when we are failing all recipients as
a result of timeout_frozen_after. If the system filter yields "delivered", then
ignore the true recipients of the message. Failure of the filter file is
logged, and the delivery attempt fails. */

else if (system_filter && process_recipients != RECIP_FAIL_TIMEOUT)
  {
  int rc;
  int filtertype;
  ugid_block ugid;
  redirect_block redirect;

  if (system_filter_uid_set)
    {
    ugid.uid = system_filter_uid;
    ugid.gid = system_filter_gid;
    ugid.uid_set = ugid.gid_set = TRUE;
    }
  else
    {
    ugid.uid_set = ugid.gid_set = FALSE;
    }

  return_path = sender_address;
  enable_dollar_recipients = TRUE;   /* Permit $recipients in system filter */
  system_filtering = TRUE;

  /* Any error in the filter file causes a delivery to be abandoned. */

  redirect.string = system_filter;
  redirect.isfile = TRUE;
  redirect.check_owner = redirect.check_group = FALSE;
  redirect.owners = NULL;
  redirect.owngroups = NULL;
  redirect.pw = NULL;
  redirect.modemask = 0;

  DEBUG(D_deliver|D_filter) debug_printf("running system filter\n");

  rc = rda_interpret(
    &redirect,              /* Where the data is */
    RDO_DEFER |             /* Turn on all the enabling options */
      RDO_FAIL |            /* Leave off all the disabling options */
      RDO_FILTER |
      RDO_FREEZE |
      RDO_REALLOG |
      RDO_REWRITE,
    NULL,                   /* No :include: restriction (not used in filter) */
    NULL,                   /* No sieve vacation directory (not sieve!) */
    NULL,                   /* No sieve enotify mailto owner (not sieve!) */
    NULL,                   /* No sieve user address (not sieve!) */
    NULL,                   /* No sieve subaddress (not sieve!) */
    &ugid,                  /* uid/gid data */
    &addr_new,              /* Where to hang generated addresses */
    &filter_message,        /* Where to put error message */
    NULL,                   /* Don't skip syntax errors */
    &filtertype,            /* Will always be set to FILTER_EXIM for this call */
    US"system filter");     /* For error messages */

  DEBUG(D_deliver|D_filter) debug_printf("system filter returned %d\n", rc);

  if (rc == FF_ERROR || rc == FF_NONEXIST)
    {
    (void)close(deliver_datafile);
    deliver_datafile = -1;
    log_write(0, LOG_MAIN|LOG_PANIC, "Error in system filter: %s",
      string_printing(filter_message));
    return continue_closedown();   /* yields DELIVER_NOT_ATTEMPTED */
    }

  /* Reset things. If the filter message is an empty string, which can happen
  for a filter "fail" or "freeze" command with no text, reset it to NULL. */

  system_filtering = FALSE;
  enable_dollar_recipients = FALSE;
  if (filter_message && filter_message[0] == 0) filter_message = NULL;

  /* Save the values of the system filter variables so that user filters
  can use them. */

  memcpy(filter_sn, filter_n, sizeof(filter_sn));

  /* The filter can request that delivery of the original addresses be
  deferred. */

  if (rc == FF_DEFER)
    {
    process_recipients = RECIP_DEFER;
    deliver_msglog("Delivery deferred by system filter\n");
    log_write(0, LOG_MAIN, "Delivery deferred by system filter");
    }

  /* The filter can request that a message be frozen, but this does not
  take place if the message has been manually thawed. In that case, we must
  unset "delivered", which is forced by the "freeze" command to make -bF
  work properly. */

  else if (rc == FF_FREEZE && !deliver_manual_thaw)
    {
    deliver_freeze = TRUE;
    deliver_frozen_at = time(NULL);
    process_recipients = RECIP_DEFER;
    frozen_info = string_sprintf(" by the system filter%s%s",
      filter_message ? US": " : US"",
      filter_message ? filter_message : US"");
    }

  /* The filter can request that a message be failed. The error message may be
  quite long - it is sent back to the sender in the bounce - but we don't want
  to fill up the log with repetitions of it. If it starts with << then the text
  between << and >> is written to the log, with the rest left for the bounce
  message. */

  else if (rc == FF_FAIL)
    {
    uschar *colon = US"";
    uschar *logmsg = US"";
    int loglen = 0;

    process_recipients = RECIP_FAIL_FILTER;

    if (filter_message)
      {
      uschar *logend;
      colon = US": ";
      if (  filter_message[0] == '<'
         && filter_message[1] == '<'
	 && (logend = Ustrstr(filter_message, ">>"))
	 )
        {
        logmsg = filter_message + 2;
        loglen = logend - logmsg;
        filter_message = logend + 2;
        if (filter_message[0] == 0) filter_message = NULL;
        }
      else
        {
        logmsg = filter_message;
        loglen = Ustrlen(filter_message);
        }
      }

    log_write(0, LOG_MAIN, "cancelled by system filter%s%.*s", colon, loglen,
      logmsg);
    }

  /* Delivery can be restricted only to those recipients (if any) that the
  filter specified. */

  else if (rc == FF_DELIVERED)
    {
    process_recipients = RECIP_IGNORE;
    if (addr_new)
      log_write(0, LOG_MAIN, "original recipients ignored (system filter)");
    else
      log_write(0, LOG_MAIN, "=> discarded (system filter)");
    }

  /* If any new addresses were created by the filter, fake up a "parent"
  for them. This is necessary for pipes, etc., which are expected to have
  parents, and it also gives some sensible logging for others. Allow
  pipes, files, and autoreplies, and run them as the filter uid if set,
  otherwise as the current uid. */

  if (addr_new)
    {
    int uid = (system_filter_uid_set)? system_filter_uid : geteuid();
    int gid = (system_filter_gid_set)? system_filter_gid : getegid();

    /* The text "system-filter" is tested in transport_set_up_command() and in
    set_up_shell_command() in the pipe transport, to enable them to permit
    $recipients, so don't change it here without also changing it there. */

    address_item *p = addr_new;
    address_item *parent = deliver_make_addr(US"system-filter", FALSE);

    parent->domain = string_copylc(qualify_domain_recipient);
    parent->local_part = US"system-filter";

    /* As part of this loop, we arrange for addr_last to end up pointing
    at the final address. This is used if we go on to add addresses for the
    original recipients. */

    while (p)
      {
      if (parent->child_count == USHRT_MAX)
        log_write(0, LOG_MAIN|LOG_PANIC_DIE, "system filter generated more "
          "than %d delivery addresses", USHRT_MAX);
      parent->child_count++;
      p->parent = parent;

      if (testflag(p, af_pfr))
        {
        uschar *tpname;
        uschar *type;
        p->uid = uid;
        p->gid = gid;
        setflag(p, af_uid_set);
        setflag(p, af_gid_set);
        setflag(p, af_allow_file);
        setflag(p, af_allow_pipe);
        setflag(p, af_allow_reply);

        /* Find the name of the system filter's appropriate pfr transport */

        if (p->address[0] == '|')
          {
          type = US"pipe";
          tpname = system_filter_pipe_transport;
          address_pipe = p->address;
          }
        else if (p->address[0] == '>')
          {
          type = US"reply";
          tpname = system_filter_reply_transport;
          }
        else
          {
          if (p->address[Ustrlen(p->address)-1] == '/')
            {
            type = US"directory";
            tpname = system_filter_directory_transport;
            }
          else
            {
            type = US"file";
            tpname = system_filter_file_transport;
            }
          address_file = p->address;
          }

        /* Now find the actual transport, first expanding the name. We have
        set address_file or address_pipe above. */

        if (tpname)
          {
          uschar *tmp = expand_string(tpname);
          address_file = address_pipe = NULL;
          if (!tmp)
            p->message = string_sprintf("failed to expand \"%s\" as a "
              "system filter transport name", tpname);
          tpname = tmp;
          }
        else
          p->message = string_sprintf("system_filter_%s_transport is unset",
            type);

        if (tpname)
          {
          transport_instance *tp;
          for (tp = transports; tp; tp = tp->next)
            if (Ustrcmp(tp->name, tpname) == 0)
              {
              p->transport = tp;
              break;
              }
          if (!tp)
            p->message = string_sprintf("failed to find \"%s\" transport "
              "for system filter delivery", tpname);
          }

        /* If we couldn't set up a transport, defer the delivery, putting the
        error on the panic log as well as the main log. */

        if (!p->transport)
          {
          address_item *badp = p;
          p = p->next;
          if (!addr_last) addr_new = p; else addr_last->next = p;
          badp->local_part = badp->address;   /* Needed for log line */
          post_process_one(badp, DEFER, LOG_MAIN|LOG_PANIC, EXIM_DTYPE_ROUTER, 0);
          continue;
          }
        }    /* End of pfr handling */

      /* Either a non-pfr delivery, or we found a transport */

      DEBUG(D_deliver|D_filter)
        debug_printf("system filter added %s\n", p->address);

      addr_last = p;
      p = p->next;
      }    /* Loop through all addr_new addresses */
    }
  }


/* Scan the recipients list, and for every one that is not in the non-
recipients tree, add an addr item to the chain of new addresses. If the pno
value is non-negative, we must set the onetime parent from it. This which
points to the relevant entry in the recipients list.

This processing can be altered by the setting of the process_recipients
variable, which is changed if recipients are to be ignored, failed, or
deferred. This can happen as a result of system filter activity, or if the -Mg
option is used to fail all of them.

Duplicate addresses are handled later by a different tree structure; we can't
just extend the non-recipients tree, because that will be re-written to the
spool if the message is deferred, and in any case there are casing
complications for local addresses. */

if (process_recipients != RECIP_IGNORE)
  for (i = 0; i < recipients_count; i++)
    if (!tree_search(tree_nonrecipients, recipients_list[i].address))
      {
      recipient_item *r = recipients_list + i;
      address_item *new = deliver_make_addr(r->address, FALSE);
      new->prop.errors_address = r->errors_to;
#ifdef SUPPORT_I18N
      if ((new->prop.utf8_msg = message_smtputf8))
	{
	new->prop.utf8_downcvt =       message_utf8_downconvert == 1;
	new->prop.utf8_downcvt_maybe = message_utf8_downconvert == -1;
	DEBUG(D_deliver) debug_printf("utf8, downconvert %s\n",
	  new->prop.utf8_downcvt ? "yes"
	  : new->prop.utf8_downcvt_maybe ? "ifneeded"
	  : "no");
	}
#endif

      if (r->pno >= 0)
        new->onetime_parent = recipients_list[r->pno].address;

      /* If DSN support is enabled, set the dsn flags and the original receipt
         to be passed on to other DSN enabled MTAs */
      new->dsn_flags = r->dsn_flags & rf_dsnflags;
      new->dsn_orcpt = r->orcpt;
      DEBUG(D_deliver) debug_printf("DSN: set orcpt: %s  flags: %d\n",
	new->dsn_orcpt ? new->dsn_orcpt : US"", new->dsn_flags);

      switch (process_recipients)
        {
        /* RECIP_DEFER is set when a system filter freezes a message. */

        case RECIP_DEFER:
        new->next = addr_defer;
        addr_defer = new;
        break;


        /* RECIP_FAIL_FILTER is set when a system filter has obeyed a "fail"
        command. */

        case RECIP_FAIL_FILTER:
        new->message =
          filter_message ? filter_message : US"delivery cancelled";
        setflag(new, af_pass_message);
        goto RECIP_QUEUE_FAILED;   /* below */


        /* RECIP_FAIL_TIMEOUT is set when a message is frozen, but is older
        than the value in timeout_frozen_after. Treat non-bounce messages
        similarly to -Mg; for bounce messages we just want to discard, so
        don't put the address on the failed list. The timeout has already
        been logged. */

        case RECIP_FAIL_TIMEOUT:
        new->message  = US"delivery cancelled; message timed out";
        goto RECIP_QUEUE_FAILED;   /* below */


        /* RECIP_FAIL is set when -Mg has been used. */

        case RECIP_FAIL:
        new->message  = US"delivery cancelled by administrator";
        /* Fall through */

        /* Common code for the failure cases above. If this is not a bounce
        message, put the address on the failed list so that it is used to
        create a bounce. Otherwise do nothing - this just discards the address.
        The incident has already been logged. */

        RECIP_QUEUE_FAILED:
        if (sender_address[0] != 0)
          {
          new->next = addr_failed;
          addr_failed = new;
          }
        break;


        /* RECIP_FAIL_LOOP is set when there are too many Received: headers
        in the message. Process each address as a routing failure; if this
        is a bounce message, it will get frozen. */

        case RECIP_FAIL_LOOP:
        new->message = US"Too many \"Received\" headers - suspected mail loop";
        post_process_one(new, FAIL, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
        break;


        /* Value should be RECIP_ACCEPT; take this as the safe default. */

        default:
        if (!addr_new) addr_new = new; else addr_last->next = new;
        addr_last = new;
        break;
        }

#ifndef DISABLE_EVENT
      if (process_recipients != RECIP_ACCEPT)
	{
	uschar * save_local =  deliver_localpart;
	const uschar * save_domain = deliver_domain;

	deliver_localpart = expand_string(
		      string_sprintf("${local_part:%s}", new->address));
	deliver_domain =    expand_string(
		      string_sprintf("${domain:%s}", new->address));

	(void) event_raise(event_action,
		      US"msg:fail:internal", new->message);

	deliver_localpart = save_local;
	deliver_domain =    save_domain;
	}
#endif
      }

DEBUG(D_deliver)
  {
  address_item *p;
  debug_printf("Delivery address list:\n");
  for (p = addr_new; p; p = p->next)
    debug_printf("  %s %s\n", p->address,
      p->onetime_parent ? p->onetime_parent : US"");
  }

/* Set up the buffers used for copying over the file when delivering. */

deliver_in_buffer = store_malloc(DELIVER_IN_BUFFER_SIZE);
deliver_out_buffer = store_malloc(DELIVER_OUT_BUFFER_SIZE);



/* Until there are no more new addresses, handle each one as follows:

 . If this is a generated address (indicated by the presence of a parent
   pointer) then check to see whether it is a pipe, file, or autoreply, and
   if so, handle it directly here. The router that produced the address will
   have set the allow flags into the address, and also set the uid/gid required.
   Having the routers generate new addresses and then checking them here at
   the outer level is tidier than making each router do the checking, and
   means that routers don't need access to the failed address queue.

 . Break up the address into local part and domain, and make lowercased
   versions of these strings. We also make unquoted versions of the local part.

 . Handle the percent hack for those domains for which it is valid.

 . For child addresses, determine if any of the parents have the same address.
   If so, generate a different string for previous delivery checking. Without
   this code, if the address spqr generates spqr via a forward or alias file,
   delivery of the generated spqr stops further attempts at the top level spqr,
   which is not what is wanted - it may have generated other addresses.

 . Check on the retry database to see if routing was previously deferred, but
   only if in a queue run. Addresses that are to be routed are put on the
   addr_route chain. Addresses that are to be deferred are put on the
   addr_defer chain. We do all the checking first, so as not to keep the
   retry database open any longer than necessary.

 . Now we run the addresses through the routers. A router may put the address
   on either the addr_local or the addr_remote chain for local or remote
   delivery, respectively, or put it on the addr_failed chain if it is
   undeliveable, or it may generate child addresses and put them on the
   addr_new chain, or it may defer an address. All the chain anchors are
   passed as arguments so that the routers can be called for verification
   purposes as well.

 . If new addresses have been generated by the routers, da capo.
*/

header_rewritten = FALSE;          /* No headers rewritten yet */
while (addr_new)           /* Loop until all addresses dealt with */
  {
  address_item *addr, *parent;

  /* Failure to open the retry database is treated the same as if it does
  not exist. In both cases, dbm_file is NULL. */

  if (!(dbm_file = dbfn_open(US"retry", O_RDONLY, &dbblock, FALSE)))
    DEBUG(D_deliver|D_retry|D_route|D_hints_lookup)
      debug_printf("no retry data available\n");

  /* Scan the current batch of new addresses, to handle pipes, files and
  autoreplies, and determine which others are ready for routing. */

  while (addr_new)
    {
    int rc;
    uschar *p;
    tree_node *tnode;
    dbdata_retry *domain_retry_record;
    dbdata_retry *address_retry_record;

    addr = addr_new;
    addr_new = addr->next;

    DEBUG(D_deliver|D_retry|D_route)
      {
      debug_printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
      debug_printf("Considering: %s\n", addr->address);
      }

    /* Handle generated address that is a pipe or a file or an autoreply. */

    if (testflag(addr, af_pfr))
      {
      /* If an autoreply in a filter could not generate a syntactically valid
      address, give up forthwith. Set af_ignore_error so that we don't try to
      generate a bounce. */

      if (testflag(addr, af_bad_reply))
        {
        addr->basic_errno = ERRNO_BADADDRESS2;
        addr->local_part = addr->address;
        addr->message =
          US"filter autoreply generated syntactically invalid recipient";
        addr->prop.ignore_error = TRUE;
        (void) post_process_one(addr, FAIL, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
        continue;   /* with the next new address */
        }

      /* If two different users specify delivery to the same pipe or file or
      autoreply, there should be two different deliveries, so build a unique
      string that incorporates the original address, and use this for
      duplicate testing and recording delivery, and also for retrying. */

      addr->unique =
        string_sprintf("%s:%s", addr->address, addr->parent->unique +
          (testflag(addr->parent, af_homonym)? 3:0));

      addr->address_retry_key = addr->domain_retry_key =
        string_sprintf("T:%s", addr->unique);

      /* If a filter file specifies two deliveries to the same pipe or file,
      we want to de-duplicate, but this is probably not wanted for two mail
      commands to the same address, where probably both should be delivered.
      So, we have to invent a different unique string in that case. Just
      keep piling '>' characters on the front. */

      if (addr->address[0] == '>')
        {
        while (tree_search(tree_duplicates, addr->unique))
          addr->unique = string_sprintf(">%s", addr->unique);
        }

      else if ((tnode = tree_search(tree_duplicates, addr->unique)))
        {
        DEBUG(D_deliver|D_route)
          debug_printf("%s is a duplicate address: discarded\n", addr->address);
        addr->dupof = tnode->data.ptr;
        addr->next = addr_duplicate;
        addr_duplicate = addr;
        continue;
        }

      DEBUG(D_deliver|D_route) debug_printf("unique = %s\n", addr->unique);

      /* Check for previous delivery */

      if (tree_search(tree_nonrecipients, addr->unique))
        {
        DEBUG(D_deliver|D_route)
          debug_printf("%s was previously delivered: discarded\n", addr->address);
        child_done(addr, tod_stamp(tod_log));
        continue;
        }

      /* Save for checking future duplicates */

      tree_add_duplicate(addr->unique, addr);

      /* Set local part and domain */

      addr->local_part = addr->address;
      addr->domain = addr->parent->domain;

      /* Ensure that the delivery is permitted. */

      if (testflag(addr, af_file))
        {
        if (!testflag(addr, af_allow_file))
          {
          addr->basic_errno = ERRNO_FORBIDFILE;
          addr->message = US"delivery to file forbidden";
          (void)post_process_one(addr, FAIL, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
          continue;   /* with the next new address */
          }
        }
      else if (addr->address[0] == '|')
        {
        if (!testflag(addr, af_allow_pipe))
          {
          addr->basic_errno = ERRNO_FORBIDPIPE;
          addr->message = US"delivery to pipe forbidden";
          (void)post_process_one(addr, FAIL, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
          continue;   /* with the next new address */
          }
        }
      else if (!testflag(addr, af_allow_reply))
        {
        addr->basic_errno = ERRNO_FORBIDREPLY;
        addr->message = US"autoreply forbidden";
        (void)post_process_one(addr, FAIL, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
        continue;     /* with the next new address */
        }

      /* If the errno field is already set to BADTRANSPORT, it indicates
      failure to expand a transport string, or find the associated transport,
      or an unset transport when one is required. Leave this test till now so
      that the forbid errors are given in preference. */

      if (addr->basic_errno == ERRNO_BADTRANSPORT)
        {
        (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
        continue;
        }

      /* Treat /dev/null as a special case and abandon the delivery. This
      avoids having to specify a uid on the transport just for this case.
      Arrange for the transport name to be logged as "**bypassed**". */

      if (Ustrcmp(addr->address, "/dev/null") == 0)
        {
        uschar *save = addr->transport->name;
        addr->transport->name = US"**bypassed**";
        (void)post_process_one(addr, OK, LOG_MAIN, EXIM_DTYPE_TRANSPORT, '=');
        addr->transport->name = save;
        continue;   /* with the next new address */
        }

      /* Pipe, file, or autoreply delivery is to go ahead as a normal local
      delivery. */

      DEBUG(D_deliver|D_route)
        debug_printf("queued for %s transport\n", addr->transport->name);
      addr->next = addr_local;
      addr_local = addr;
      continue;       /* with the next new address */
      }

    /* Handle normal addresses. First, split up into local part and domain,
    handling the %-hack if necessary. There is the possibility of a defer from
    a lookup in percent_hack_domains. */

    if ((rc = deliver_split_address(addr)) == DEFER)
      {
      addr->message = US"cannot check percent_hack_domains";
      addr->basic_errno = ERRNO_LISTDEFER;
      (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_NONE, 0);
      continue;
      }

    /* Check to see if the domain is held. If so, proceed only if the
    delivery was forced by hand. */

    deliver_domain = addr->domain;  /* set $domain */
    if (  !forced && hold_domains
       && (rc = match_isinlist(addr->domain, (const uschar **)&hold_domains, 0,
           &domainlist_anchor, addr->domain_cache, MCL_DOMAIN, TRUE,
           NULL)) != FAIL
       )
      {
      if (rc == DEFER)
        {
        addr->message = US"hold_domains lookup deferred";
        addr->basic_errno = ERRNO_LISTDEFER;
        }
      else
        {
        addr->message = US"domain is held";
        addr->basic_errno = ERRNO_HELD;
        }
      (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_NONE, 0);
      continue;
      }

    /* Now we can check for duplicates and previously delivered addresses. In
    order to do this, we have to generate a "unique" value for each address,
    because there may be identical actual addresses in a line of descendents.
    The "unique" field is initialized to the same value as the "address" field,
    but gets changed here to cope with identically-named descendents. */

    for (parent = addr->parent; parent; parent = parent->parent)
      if (strcmpic(addr->address, parent->address) == 0) break;

    /* If there's an ancestor with the same name, set the homonym flag. This
    influences how deliveries are recorded. Then add a prefix on the front of
    the unique address. We use \n\ where n starts at 0 and increases each time.
    It is unlikely to pass 9, but if it does, it may look odd but will still
    work. This means that siblings or cousins with the same names are treated
    as duplicates, which is what we want. */

    if (parent)
      {
      setflag(addr, af_homonym);
      if (parent->unique[0] != '\\')
        addr->unique = string_sprintf("\\0\\%s", addr->address);
      else
        addr->unique = string_sprintf("\\%c\\%s", parent->unique[1] + 1,
          addr->address);
      }

    /* Ensure that the domain in the unique field is lower cased, because
    domains are always handled caselessly. */

    p = Ustrrchr(addr->unique, '@');
    while (*p != 0) { *p = tolower(*p); p++; }

    DEBUG(D_deliver|D_route) debug_printf("unique = %s\n", addr->unique);

    if (tree_search(tree_nonrecipients, addr->unique))
      {
      DEBUG(D_deliver|D_route)
        debug_printf("%s was previously delivered: discarded\n", addr->unique);
      child_done(addr, tod_stamp(tod_log));
      continue;
      }

    /* Get the routing retry status, saving the two retry keys (with and
    without the local part) for subsequent use. If there is no retry record for
    the standard address routing retry key, we look for the same key with the
    sender attached, because this form is used by the smtp transport after a
    4xx response to RCPT when address_retry_include_sender is true. */

    addr->domain_retry_key = string_sprintf("R:%s", addr->domain);
    addr->address_retry_key = string_sprintf("R:%s@%s", addr->local_part,
      addr->domain);

    if (dbm_file)
      {
      domain_retry_record = dbfn_read(dbm_file, addr->domain_retry_key);
      if (  domain_retry_record
         && now - domain_retry_record->time_stamp > retry_data_expire
	 )
        domain_retry_record = NULL;    /* Ignore if too old */

      address_retry_record = dbfn_read(dbm_file, addr->address_retry_key);
      if (  address_retry_record
         && now - address_retry_record->time_stamp > retry_data_expire
	 )
        address_retry_record = NULL;   /* Ignore if too old */

      if (!address_retry_record)
        {
        uschar *altkey = string_sprintf("%s:<%s>", addr->address_retry_key,
          sender_address);
        address_retry_record = dbfn_read(dbm_file, altkey);
        if (  address_retry_record
	   && now - address_retry_record->time_stamp > retry_data_expire)
          address_retry_record = NULL;   /* Ignore if too old */
        }
      }
    else
      domain_retry_record = address_retry_record = NULL;

    DEBUG(D_deliver|D_retry)
      {
      if (!domain_retry_record)
        debug_printf("no domain retry record\n");
      if (!address_retry_record)
        debug_printf("no address retry record\n");
      }

    /* If we are sending a message down an existing SMTP connection, we must
    assume that the message which created the connection managed to route
    an address to that connection. We do not want to run the risk of taking
    a long time over routing here, because if we do, the server at the other
    end of the connection may time it out. This is especially true for messages
    with lots of addresses. For this kind of delivery, queue_running is not
    set, so we would normally route all addresses. We take a pragmatic approach
    and defer routing any addresses that have any kind of domain retry record.
    That is, we don't even look at their retry times. It doesn't matter if this
    doesn't work occasionally. This is all just an optimization, after all.

    The reason for not doing the same for address retries is that they normally
    arise from 4xx responses, not DNS timeouts. */

    if (continue_hostname && domain_retry_record)
      {
      addr->message = US"reusing SMTP connection skips previous routing defer";
      addr->basic_errno = ERRNO_RRETRY;
      (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
      }

    /* If we are in a queue run, defer routing unless there is no retry data or
    we've passed the next retry time, or this message is forced. In other
    words, ignore retry data when not in a queue run.

    However, if the domain retry time has expired, always allow the routing
    attempt. If it fails again, the address will be failed. This ensures that
    each address is routed at least once, even after long-term routing
    failures.

    If there is an address retry, check that too; just wait for the next
    retry time. This helps with the case when the temporary error on the
    address was really message-specific rather than address specific, since
    it allows other messages through.

    We also wait for the next retry time if this is a message sent down an
    existing SMTP connection (even though that will be forced). Otherwise there
    will be far too many attempts for an address that gets a 4xx error. In
    fact, after such an error, we should not get here because, the host should
    not be remembered as one this message needs. However, there was a bug that
    used to cause this to  happen, so it is best to be on the safe side.

    Even if we haven't reached the retry time in the hints, there is one more
    check to do, which is for the ultimate address timeout. We only do this
    check if there is an address retry record and there is not a domain retry
    record; this implies that previous attempts to handle the address had the
    retry_use_local_parts option turned on. We use this as an approximation
    for the destination being like a local delivery, for example delivery over
    LMTP to an IMAP message store. In this situation users are liable to bump
    into their quota and thereby have intermittently successful deliveries,
    which keep the retry record fresh, which can lead to us perpetually
    deferring messages. */

    else if (  (  queue_running && !deliver_force
	       || continue_hostname
	       )
            && (  (  domain_retry_record
		  && now < domain_retry_record->next_try
		  && !domain_retry_record->expired
		  )
	       || (  address_retry_record
		  && now < address_retry_record->next_try
	       )  )
            && (  domain_retry_record
	       || !address_retry_record
	       || !retry_ultimate_address_timeout(addr->address_retry_key,
				 addr->domain, address_retry_record, now)
	    )  )
      {
      addr->message = US"retry time not reached";
      addr->basic_errno = ERRNO_RRETRY;
      (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
      }

    /* The domain is OK for routing. Remember if retry data exists so it
    can be cleaned up after a successful delivery. */

    else
      {
      if (domain_retry_record || address_retry_record)
        setflag(addr, af_dr_retry_exists);
      addr->next = addr_route;
      addr_route = addr;
      DEBUG(D_deliver|D_route)
        debug_printf("%s: queued for routing\n", addr->address);
      }
    }

  /* The database is closed while routing is actually happening. Requests to
  update it are put on a chain and all processed together at the end. */

  if (dbm_file) dbfn_close(dbm_file);

  /* If queue_domains is set, we don't even want to try routing addresses in
  those domains. During queue runs, queue_domains is forced to be unset.
  Optimize by skipping this pass through the addresses if nothing is set. */

  if (!deliver_force && queue_domains)
    {
    address_item *okaddr = NULL;
    while (addr_route)
      {
      address_item *addr = addr_route;
      addr_route = addr->next;

      deliver_domain = addr->domain;  /* set $domain */
      if ((rc = match_isinlist(addr->domain, (const uschar **)&queue_domains, 0,
            &domainlist_anchor, addr->domain_cache, MCL_DOMAIN, TRUE, NULL))
              != OK)
        if (rc == DEFER)
          {
          addr->basic_errno = ERRNO_LISTDEFER;
          addr->message = US"queue_domains lookup deferred";
          (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
          }
        else
          {
          addr->next = okaddr;
          okaddr = addr;
          }
      else
        {
        addr->basic_errno = ERRNO_QUEUE_DOMAIN;
        addr->message = US"domain is in queue_domains";
        (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
        }
      }

    addr_route = okaddr;
    }

  /* Now route those addresses that are not deferred. */

  while (addr_route)
    {
    int rc;
    address_item *addr = addr_route;
    const uschar *old_domain = addr->domain;
    uschar *old_unique = addr->unique;
    addr_route = addr->next;
    addr->next = NULL;

    /* Just in case some router parameter refers to it. */

    if (!(return_path = addr->prop.errors_address))
      return_path = sender_address;

    /* If a router defers an address, add a retry item. Whether or not to
    use the local part in the key is a property of the router. */

    if ((rc = route_address(addr, &addr_local, &addr_remote, &addr_new,
         &addr_succeed, v_none)) == DEFER)
      retry_add_item(addr,
        addr->router->retry_use_local_part
        ? string_sprintf("R:%s@%s", addr->local_part, addr->domain)
	: string_sprintf("R:%s", addr->domain),
	0);

    /* Otherwise, if there is an existing retry record in the database, add
    retry items to delete both forms. We must also allow for the possibility
    of a routing retry that includes the sender address. Since the domain might
    have been rewritten (expanded to fully qualified) as a result of routing,
    ensure that the rewritten form is also deleted. */

    else if (testflag(addr, af_dr_retry_exists))
      {
      uschar *altkey = string_sprintf("%s:<%s>", addr->address_retry_key,
        sender_address);
      retry_add_item(addr, altkey, rf_delete);
      retry_add_item(addr, addr->address_retry_key, rf_delete);
      retry_add_item(addr, addr->domain_retry_key, rf_delete);
      if (Ustrcmp(addr->domain, old_domain) != 0)
        retry_add_item(addr, string_sprintf("R:%s", old_domain), rf_delete);
      }

    /* DISCARD is given for :blackhole: and "seen finish". The event has been
    logged, but we need to ensure the address (and maybe parents) is marked
    done. */

    if (rc == DISCARD)
      {
      address_done(addr, tod_stamp(tod_log));
      continue;  /* route next address */
      }

    /* The address is finished with (failed or deferred). */

    if (rc != OK)
      {
      (void)post_process_one(addr, rc, LOG_MAIN, EXIM_DTYPE_ROUTER, 0);
      continue;  /* route next address */
      }

    /* The address has been routed. If the router changed the domain, it will
    also have changed the unique address. We have to test whether this address
    has already been delivered, because it's the unique address that finally
    gets recorded. */

    if (  addr->unique != old_unique
       && tree_search(tree_nonrecipients, addr->unique) != 0
       )
      {
      DEBUG(D_deliver|D_route) debug_printf("%s was previously delivered: "
        "discarded\n", addr->address);
      if (addr_remote == addr) addr_remote = addr->next;
      else if (addr_local == addr) addr_local = addr->next;
      }

    /* If the router has same_domain_copy_routing set, we are permitted to copy
    the routing for any other addresses with the same domain. This is an
    optimisation to save repeated DNS lookups for "standard" remote domain
    routing. The option is settable only on routers that generate host lists.
    We play it very safe, and do the optimization only if the address is routed
    to a remote transport, there are no header changes, and the domain was not
    modified by the router. */

    if (  addr_remote == addr
       && addr->router->same_domain_copy_routing
       && !addr->prop.extra_headers
       && !addr->prop.remove_headers
       && old_domain == addr->domain
       )
      {
      address_item **chain = &addr_route;
      while (*chain)
        {
        address_item *addr2 = *chain;
        if (Ustrcmp(addr2->domain, addr->domain) != 0)
          {
          chain = &(addr2->next);
          continue;
          }

        /* Found a suitable address; take it off the routing list and add it to
        the remote delivery list. */

        *chain = addr2->next;
        addr2->next = addr_remote;
        addr_remote = addr2;

        /* Copy the routing data */

        addr2->domain = addr->domain;
        addr2->router = addr->router;
        addr2->transport = addr->transport;
        addr2->host_list = addr->host_list;
        addr2->fallback_hosts = addr->fallback_hosts;
        addr2->prop.errors_address = addr->prop.errors_address;
        copyflag(addr2, addr, af_hide_child);
        copyflag(addr2, addr, af_local_host_removed);

        DEBUG(D_deliver|D_route)
          debug_printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n"
                       "routing %s\n"
                       "Routing for %s copied from %s\n",
            addr2->address, addr2->address, addr->address);
        }
      }
    }  /* Continue with routing the next address. */
  }    /* Loop to process any child addresses that the routers created, and
          any rerouted addresses that got put back on the new chain. */


/* Debugging: show the results of the routing */

DEBUG(D_deliver|D_retry|D_route)
  {
  address_item *p;
  debug_printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
  debug_printf("After routing:\n  Local deliveries:\n");
  for (p = addr_local; p; p = p->next)
    debug_printf("    %s\n", p->address);

  debug_printf("  Remote deliveries:\n");
  for (p = addr_remote; p; p = p->next)
    debug_printf("    %s\n", p->address);

  debug_printf("  Failed addresses:\n");
  for (p = addr_failed; p; p = p->next)
    debug_printf("    %s\n", p->address);

  debug_printf("  Deferred addresses:\n");
  for (p = addr_defer; p; p = p->next)
    debug_printf("    %s\n", p->address);
  }

/* Free any resources that were cached during routing. */

search_tidyup();
route_tidyup();

/* These two variables are set only during routing, after check_local_user.
Ensure they are not set in transports. */

local_user_gid = (gid_t)(-1);
local_user_uid = (uid_t)(-1);

/* Check for any duplicate addresses. This check is delayed until after
routing, because the flexibility of the routing configuration means that
identical addresses with different parentage may end up being redirected to
different addresses. Checking for duplicates too early (as we previously used
to) makes this kind of thing not work. */

do_duplicate_check(&addr_local);
do_duplicate_check(&addr_remote);

/* When acting as an MUA wrapper, we proceed only if all addresses route to a
remote transport. The check that they all end up in one transaction happens in
the do_remote_deliveries() function. */

if (  mua_wrapper
   && (addr_local || addr_failed || addr_defer)
   )
  {
  address_item *addr;
  uschar *which, *colon, *msg;

  if (addr_local)
    {
    addr = addr_local;
    which = US"local";
    }
  else if (addr_defer)
    {
    addr = addr_defer;
    which = US"deferred";
    }
  else
    {
    addr = addr_failed;
    which = US"failed";
    }

  while (addr->parent) addr = addr->parent;

  if (addr->message)
    {
    colon = US": ";
    msg = addr->message;
    }
  else colon = msg = US"";

  /* We don't need to log here for a forced failure as it will already
  have been logged. Defer will also have been logged, but as a defer, so we do
  need to do the failure logging. */

  if (addr != addr_failed)
    log_write(0, LOG_MAIN, "** %s routing yielded a %s delivery",
      addr->address, which);

  /* Always write an error to the caller */

  fprintf(stderr, "routing %s yielded a %s delivery%s%s\n", addr->address,
    which, colon, msg);

  final_yield = DELIVER_MUA_FAILED;
  addr_failed = addr_defer = NULL;   /* So that we remove the message */
  goto DELIVERY_TIDYUP;
  }


/* If this is a run to continue deliveries to an external channel that is
already set up, defer any local deliveries. */

if (continue_transport)
  {
  if (addr_defer)
    {
    address_item *addr = addr_defer;
    while (addr->next) addr = addr->next;
    addr->next = addr_local;
    }
  else
    addr_defer = addr_local;
  addr_local = NULL;
  }


/* Because address rewriting can happen in the routers, we should not really do
ANY deliveries until all addresses have been routed, so that all recipients of
the message get the same headers. However, this is in practice not always
possible, since sometimes remote addresses give DNS timeouts for days on end.
The pragmatic approach is to deliver what we can now, saving any rewritten
headers so that at least the next lot of recipients benefit from the rewriting
that has already been done.

If any headers have been rewritten during routing, update the spool file to
remember them for all subsequent deliveries. This can be delayed till later if
there is only address to be delivered - if it succeeds the spool write need not
happen. */

if (  header_rewritten
   && (  addr_local && (addr_local->next || addr_remote)
      || addr_remote && addr_remote->next
   )  )
  {
  /* Panic-dies on error */
  (void)spool_write_header(message_id, SW_DELIVERING, NULL);
  header_rewritten = FALSE;
  }


/* If there are any deliveries to be and we do not already have the journal
file, create it. This is used to record successful deliveries as soon as
possible after each delivery is known to be complete. A file opened with
O_APPEND is used so that several processes can run simultaneously.

The journal is just insurance against crashes. When the spool file is
ultimately updated at the end of processing, the journal is deleted. If a
journal is found to exist at the start of delivery, the addresses listed
therein are added to the non-recipients. */

if (addr_local || addr_remote)
  {
  if (journal_fd < 0)
    {
    uschar * fname = spool_fname(US"input", message_subdir, id, US"-J");

    if ((journal_fd = Uopen(fname,
#ifdef O_CLOEXEC
			O_CLOEXEC |
#endif
			O_WRONLY|O_APPEND|O_CREAT|O_EXCL, SPOOL_MODE)) < 0)
      {
      log_write(0, LOG_MAIN|LOG_PANIC, "Couldn't open journal file %s: %s",
	fname, strerror(errno));
      return DELIVER_NOT_ATTEMPTED;
      }

    /* Set the close-on-exec flag, make the file owned by Exim, and ensure
    that the mode is correct - the group setting doesn't always seem to get
    set automatically. */

    if(  fchown(journal_fd, exim_uid, exim_gid)
      || fchmod(journal_fd, SPOOL_MODE)
#ifndef O_CLOEXEC
      || fcntl(journal_fd, F_SETFD, fcntl(journal_fd, F_GETFD) | FD_CLOEXEC)
#endif
      )
      {
      int ret = Uunlink(fname);
      log_write(0, LOG_MAIN|LOG_PANIC, "Couldn't set perms on journal file %s: %s",
	fname, strerror(errno));
      if(ret  &&  errno != ENOENT)
	log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to unlink %s: %s",
	  fname, strerror(errno));
      return DELIVER_NOT_ATTEMPTED;
      }
    }
  }
else if (journal_fd >= 0)
  {
  close(journal_fd);
  journal_fd = -1;
  }



/* Now we can get down to the business of actually doing deliveries. Local
deliveries are done first, then remote ones. If ever the problems of how to
handle fallback transports are figured out, this section can be put into a loop
for handling fallbacks, though the uid switching will have to be revised. */

/* Precompile a regex that is used to recognize a parameter in response
to an LHLO command, if is isn't already compiled. This may be used on both
local and remote LMTP deliveries. */

if (!regex_IGNOREQUOTA)
  regex_IGNOREQUOTA =
    regex_must_compile(US"\\n250[\\s\\-]IGNOREQUOTA(\\s|\\n|$)", FALSE, TRUE);

/* Handle local deliveries */

if (addr_local)
  {
  DEBUG(D_deliver|D_transport)
    debug_printf(">>>>>>>>>>>>>>>> Local deliveries >>>>>>>>>>>>>>>>\n");
  do_local_deliveries();
  disable_logging = FALSE;
  }

/* If queue_run_local is set, we do not want to attempt any remote deliveries,
so just queue them all. */

if (queue_run_local)
  while (addr_remote)
    {
    address_item *addr = addr_remote;
    addr_remote = addr->next;
    addr->next = NULL;
    addr->basic_errno = ERRNO_LOCAL_ONLY;
    addr->message = US"remote deliveries suppressed";
    (void)post_process_one(addr, DEFER, LOG_MAIN, EXIM_DTYPE_TRANSPORT, 0);
    }

/* Handle remote deliveries */

if (addr_remote)
  {
  DEBUG(D_deliver|D_transport)
    debug_printf(">>>>>>>>>>>>>>>> Remote deliveries >>>>>>>>>>>>>>>>\n");

  /* Precompile some regex that are used to recognize parameters in response
  to an EHLO command, if they aren't already compiled. */

  deliver_init();

  /* Now sort the addresses if required, and do the deliveries. The yield of
  do_remote_deliveries is FALSE when mua_wrapper is set and all addresses
  cannot be delivered in one transaction. */

  if (remote_sort_domains) sort_remote_deliveries();
  if (!do_remote_deliveries(FALSE))
    {
    log_write(0, LOG_MAIN, "** mua_wrapper is set but recipients cannot all "
      "be delivered in one transaction");
    fprintf(stderr, "delivery to smarthost failed (configuration problem)\n");

    final_yield = DELIVER_MUA_FAILED;
    addr_failed = addr_defer = NULL;   /* So that we remove the message */
    goto DELIVERY_TIDYUP;
    }

  /* See if any of the addresses that failed got put on the queue for delivery
  to their fallback hosts. We do it this way because often the same fallback
  host is used for many domains, so all can be sent in a single transaction
  (if appropriately configured). */

  if (addr_fallback && !mua_wrapper)
    {
    DEBUG(D_deliver) debug_printf("Delivering to fallback hosts\n");
    addr_remote = addr_fallback;
    addr_fallback = NULL;
    if (remote_sort_domains) sort_remote_deliveries();
    do_remote_deliveries(TRUE);
    }
  disable_logging = FALSE;
  }


/* All deliveries are now complete. Ignore SIGTERM during this tidying up
phase, to minimize cases of half-done things. */

DEBUG(D_deliver)
  debug_printf(">>>>>>>>>>>>>>>> deliveries are done >>>>>>>>>>>>>>>>\n");
cancel_cutthrough_connection(TRUE, US"deliveries are done");

/* Root privilege is no longer needed */

exim_setugid(exim_uid, exim_gid, FALSE, US"post-delivery tidying");

set_process_info("tidying up after delivering %s", message_id);
signal(SIGTERM, SIG_IGN);

/* When we are acting as an MUA wrapper, the smtp transport will either have
succeeded for all addresses, or failed them all in normal cases. However, there
are some setup situations (e.g. when a named port does not exist) that cause an
immediate exit with deferral of all addresses. Convert those into failures. We
do not ever want to retry, nor do we want to send a bounce message. */

if (mua_wrapper)
  {
  if (addr_defer)
    {
    address_item *addr, *nextaddr;
    for (addr = addr_defer; addr; addr = nextaddr)
      {
      log_write(0, LOG_MAIN, "** %s mua_wrapper forced failure for deferred "
        "delivery", addr->address);
      nextaddr = addr->next;
      addr->next = addr_failed;
      addr_failed = addr;
      }
    addr_defer = NULL;
    }

  /* Now all should either have succeeded or failed. */

  if (!addr_failed)
    final_yield = DELIVER_MUA_SUCCEEDED;
  else
    {
    host_item * host;
    uschar *s = addr_failed->user_message;

    if (!s) s = addr_failed->message;

    fprintf(stderr, "Delivery failed: ");
    if (addr_failed->basic_errno > 0)
      {
      fprintf(stderr, "%s", strerror(addr_failed->basic_errno));
      if (s) fprintf(stderr, ": ");
      }
    if ((host = addr_failed->host_used))
      fprintf(stderr, "H=%s [%s]: ", host->name, host->address);
    if (s)
      fprintf(stderr, "%s", CS s);
    else if (addr_failed->basic_errno <= 0)
      fprintf(stderr, "unknown error");
    fprintf(stderr, "\n");

    final_yield = DELIVER_MUA_FAILED;
    addr_failed = NULL;
    }
  }

/* In a normal configuration, we now update the retry database. This is done in
one fell swoop at the end in order not to keep opening and closing (and
locking) the database. The code for handling retries is hived off into a
separate module for convenience. We pass it the addresses of the various
chains, because deferred addresses can get moved onto the failed chain if the
retry cutoff time has expired for all alternative destinations. Bypass the
updating of the database if the -N flag is set, which is a debugging thing that
prevents actual delivery. */

else if (!dont_deliver)
  retry_update(&addr_defer, &addr_failed, &addr_succeed);

/* Send DSN for successful messages if requested */
addr_senddsn = NULL;

for (addr_dsntmp = addr_succeed; addr_dsntmp; addr_dsntmp = addr_dsntmp->next)
  {
  /* af_ignore_error not honored here. it's not an error */
  DEBUG(D_deliver) debug_printf("DSN: processing router : %s\n"
      "DSN: processing successful delivery address: %s\n"
      "DSN: Sender_address: %s\n"
      "DSN: orcpt: %s  flags: %d\n"
      "DSN: envid: %s  ret: %d\n"
      "DSN: Final recipient: %s\n"
      "DSN: Remote SMTP server supports DSN: %d\n",
      addr_dsntmp->router ? addr_dsntmp->router->name : US"(unknown)",
      addr_dsntmp->address,
      sender_address,
      addr_dsntmp->dsn_orcpt ? addr_dsntmp->dsn_orcpt : US"NULL",
      addr_dsntmp->dsn_flags,
      dsn_envid ? dsn_envid : US"NULL", dsn_ret,
      addr_dsntmp->address,
      addr_dsntmp->dsn_aware
      );

  /* send report if next hop not DSN aware or a router flagged "last DSN hop"
     and a report was requested */
  if (  (  addr_dsntmp->dsn_aware != dsn_support_yes
	|| addr_dsntmp->dsn_flags & rf_dsnlasthop
        )
     && addr_dsntmp->dsn_flags & rf_dsnflags
     && addr_dsntmp->dsn_flags & rf_notify_success
     )
    {
    /* copy and relink address_item and send report with all of them at once later */
    address_item * addr_next = addr_senddsn;
    addr_senddsn = store_get(sizeof(address_item));
    *addr_senddsn = *addr_dsntmp;
    addr_senddsn->next = addr_next;
    }
  else
    DEBUG(D_deliver) debug_printf("DSN: not sending DSN success message\n");
  }

if (addr_senddsn)
  {
  pid_t pid;
  int fd;

  /* create exim process to send message */
  pid = child_open_exim(&fd);

  DEBUG(D_deliver) debug_printf("DSN: child_open_exim returns: %d\n", pid);

  if (pid < 0)  /* Creation of child failed */
    {
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "Process %d (parent %d) failed to "
      "create child process to send failure message: %s", getpid(),
      getppid(), strerror(errno));

    DEBUG(D_deliver) debug_printf("DSN: child_open_exim failed\n");
    }
  else  /* Creation of child succeeded */
    {
    FILE *f = fdopen(fd, "wb");
    /* header only as required by RFC. only failure DSN needs to honor RET=FULL */
    uschar * bound;
    transport_ctx tctx = {{0}};

    DEBUG(D_deliver)
      debug_printf("sending error message to: %s\n", sender_address);

    /* build unique id for MIME boundary */
    bound = string_sprintf(TIME_T_FMT "-eximdsn-%d", time(NULL), rand());
    DEBUG(D_deliver) debug_printf("DSN: MIME boundary: %s\n", bound);

    if (errors_reply_to)
      fprintf(f, "Reply-To: %s\n", errors_reply_to);

    fprintf(f, "Auto-Submitted: auto-generated\n"
	"From: Mail Delivery System <Mailer-Daemon@%s>\n"
	"To: %s\n"
	"Subject: Delivery Status Notification\n"
	"Content-Type: multipart/report; report-type=delivery-status; boundary=%s\n"
	"MIME-Version: 1.0\n\n"

	"--%s\n"
	"Content-type: text/plain; charset=us-ascii\n\n"

	"This message was created automatically by mail delivery software.\n"
	" ----- The following addresses had successful delivery notifications -----\n",
      qualify_domain_sender, sender_address, bound, bound);

    for (addr_dsntmp = addr_senddsn; addr_dsntmp;
	 addr_dsntmp = addr_dsntmp->next)
      fprintf(f, "<%s> (relayed %s)\n\n",
	addr_dsntmp->address,
	(addr_dsntmp->dsn_flags & rf_dsnlasthop) == 1
	  ? "via non DSN router"
	  : addr_dsntmp->dsn_aware == dsn_support_no
	  ? "to non-DSN-aware mailer"
	  : "via non \"Remote SMTP\" router"
	);

    fprintf(f, "--%s\n"
	"Content-type: message/delivery-status\n\n"
	"Reporting-MTA: dns; %s\n",
      bound, smtp_active_hostname);

    if (dsn_envid)
      {			/* must be decoded from xtext: see RFC 3461:6.3a */
      uschar *xdec_envid;
      if (auth_xtextdecode(dsn_envid, &xdec_envid) > 0)
        fprintf(f, "Original-Envelope-ID: %s\n", dsn_envid);
      else
        fprintf(f, "X-Original-Envelope-ID: error decoding xtext formatted ENVID\n");
      }
    fputc('\n', f);

    for (addr_dsntmp = addr_senddsn;
	 addr_dsntmp;
	 addr_dsntmp = addr_dsntmp->next)
      {
      if (addr_dsntmp->dsn_orcpt)
        fprintf(f,"Original-Recipient: %s\n", addr_dsntmp->dsn_orcpt);

      fprintf(f, "Action: delivered\n"
	  "Final-Recipient: rfc822;%s\n"
	  "Status: 2.0.0\n",
	addr_dsntmp->address);

      if (addr_dsntmp->host_used && addr_dsntmp->host_used->name)
        fprintf(f, "Remote-MTA: dns; %s\nDiagnostic-Code: smtp; 250 Ok\n\n",
	  addr_dsntmp->host_used->name);
      else
	fprintf(f, "Diagnostic-Code: X-Exim; relayed via non %s router\n\n",
	  (addr_dsntmp->dsn_flags & rf_dsnlasthop) == 1 ? "DSN" : "SMTP");
      }

    fprintf(f, "--%s\nContent-type: text/rfc822-headers\n\n", bound);

    fflush(f);
    transport_filter_argv = NULL;   /* Just in case */
    return_path = sender_address;   /* In case not previously set */

    /* Write the original email out */

    tctx.u.fd = fileno(f);
    tctx.options = topt_add_return_path | topt_no_body;
    transport_write_message(&tctx, 0);
    fflush(f);

    fprintf(f,"\n--%s--\n", bound);

    fflush(f);
    fclose(f);
    rc = child_close(pid, 0);     /* Waits for child to close, no timeout */
    }
  }

/* If any addresses failed, we must send a message to somebody, unless
af_ignore_error is set, in which case no action is taken. It is possible for
several messages to get sent if there are addresses with different
requirements. */

while (addr_failed)
  {
  pid_t pid;
  int fd;
  uschar *logtod = tod_stamp(tod_log);
  address_item *addr;
  address_item *handled_addr = NULL;
  address_item **paddr;
  address_item *msgchain = NULL;
  address_item **pmsgchain = &msgchain;

  /* There are weird cases when logging is disabled in the transport. However,
  there may not be a transport (address failed by a router). */

  disable_logging = FALSE;
  if (addr_failed->transport)
    disable_logging = addr_failed->transport->disable_logging;

  DEBUG(D_deliver)
    debug_printf("processing failed address %s\n", addr_failed->address);

  /* There are only two ways an address in a bounce message can get here:

  (1) When delivery was initially deferred, but has now timed out (in the call
      to retry_update() above). We can detect this by testing for
      af_retry_timedout. If the address does not have its own errors address,
      we arrange to ignore the error.

  (2) If delivery failures for bounce messages are being ignored. We can detect
      this by testing for af_ignore_error. This will also be set if a bounce
      message has been autothawed and the ignore_bounce_errors_after time has
      passed. It might also be set if a router was explicitly configured to
      ignore errors (errors_to = "").

  If neither of these cases obtains, something has gone wrong. Log the
  incident, but then ignore the error. */

  if (sender_address[0] == 0 && !addr_failed->prop.errors_address)
    {
    if (  !testflag(addr_failed, af_retry_timedout)
       && !addr_failed->prop.ignore_error)
      log_write(0, LOG_MAIN|LOG_PANIC, "internal error: bounce message "
        "failure is neither frozen nor ignored (it's been ignored)");

    addr_failed->prop.ignore_error = TRUE;
    }

  /* If the first address on the list has af_ignore_error set, just remove
  it from the list, throw away any saved message file, log it, and
  mark the recipient done. */

  if (  addr_failed->prop.ignore_error
     || (  addr_failed->dsn_flags & rf_dsnflags
        && (addr_failed->dsn_flags & rf_notify_failure) != rf_notify_failure
     )  )
    {
    addr = addr_failed;
    addr_failed = addr->next;
    if (addr->return_filename) Uunlink(addr->return_filename);

    log_write(0, LOG_MAIN, "%s%s%s%s: error ignored",
      addr->address,
      !addr->parent ? US"" : US" <",
      !addr->parent ? US"" : addr->parent->address,
      !addr->parent ? US"" : US">");

    address_done(addr, logtod);
    child_done(addr, logtod);
    /* Panic-dies on error */
    (void)spool_write_header(message_id, SW_DELIVERING, NULL);
    }

  /* Otherwise, handle the sending of a message. Find the error address for
  the first address, then send a message that includes all failed addresses
  that have the same error address. Note the bounce_recipient is a global so
  that it can be accessed by $bounce_recipient while creating a customized
  error message. */

  else
    {
    if (!(bounce_recipient = addr_failed->prop.errors_address))
      bounce_recipient = sender_address;

    /* Make a subprocess to send a message */

    if ((pid = child_open_exim(&fd)) < 0)
      log_write(0, LOG_MAIN|LOG_PANIC_DIE, "Process %d (parent %d) failed to "
        "create child process to send failure message: %s", getpid(),
        getppid(), strerror(errno));

    /* Creation of child succeeded */

    else
      {
      int ch, rc;
      int filecount = 0;
      int rcount = 0;
      uschar *bcc, *emf_text;
      FILE *f = fdopen(fd, "wb");
      FILE *emf = NULL;
      BOOL to_sender = strcmpic(sender_address, bounce_recipient) == 0;
      int max = (bounce_return_size_limit/DELIVER_IN_BUFFER_SIZE + 1) *
        DELIVER_IN_BUFFER_SIZE;
      uschar * bound;
      uschar *dsnlimitmsg;
      uschar *dsnnotifyhdr;
      int topt;

      DEBUG(D_deliver)
        debug_printf("sending error message to: %s\n", bounce_recipient);

      /* Scan the addresses for all that have the same errors address, removing
      them from the addr_failed chain, and putting them on msgchain. */

      paddr = &addr_failed;
      for (addr = addr_failed; addr; addr = *paddr)
        if (Ustrcmp(bounce_recipient, addr->prop.errors_address
	      ? addr->prop.errors_address : sender_address) == 0)
          {                          /* The same - dechain */
          *paddr = addr->next;
          *pmsgchain = addr;
          addr->next = NULL;
          pmsgchain = &(addr->next);
          }
        else
          paddr = &addr->next;        /* Not the same; skip */

      /* Include X-Failed-Recipients: for automatic interpretation, but do
      not let any one header line get too long. We do this by starting a
      new header every 50 recipients. Omit any addresses for which the
      "hide_child" flag is set. */

      for (addr = msgchain; addr; addr = addr->next)
        {
        if (testflag(addr, af_hide_child)) continue;
        if (rcount >= 50)
          {
          fprintf(f, "\n");
          rcount = 0;
          }
        fprintf(f, "%s%s",
          rcount++ == 0
	  ? "X-Failed-Recipients: "
	  : ",\n  ",
          testflag(addr, af_pfr) && addr->parent
	  ? string_printing(addr->parent->address)
	  : string_printing(addr->address));
        }
      if (rcount > 0) fprintf(f, "\n");

      /* Output the standard headers */

      if (errors_reply_to)
        fprintf(f, "Reply-To: %s\n", errors_reply_to);
      fprintf(f, "Auto-Submitted: auto-replied\n");
      moan_write_from(f);
      fprintf(f, "To: %s\n", bounce_recipient);

      /* generate boundary string and output MIME-Headers */
      bound = string_sprintf(TIME_T_FMT "-eximdsn-%d", time(NULL), rand());

      fprintf(f, "Content-Type: multipart/report;"
	    " report-type=delivery-status; boundary=%s\n"
	  "MIME-Version: 1.0\n",
	bound);

      /* Open a template file if one is provided. Log failure to open, but
      carry on - default texts will be used. */

      if (bounce_message_file)
        if (!(emf = Ufopen(bounce_message_file, "rb")))
          log_write(0, LOG_MAIN|LOG_PANIC, "Failed to open %s for error "
            "message texts: %s", bounce_message_file, strerror(errno));

      /* Quietly copy to configured additional addresses if required. */

      if ((bcc = moan_check_errorcopy(bounce_recipient)))
	fprintf(f, "Bcc: %s\n", bcc);

      /* The texts for the message can be read from a template file; if there
      isn't one, or if it is too short, built-in texts are used. The first
      emf text is a Subject: and any other headers. */

      if ((emf_text = next_emf(emf, US"header")))
	fprintf(f, "%s\n", emf_text);
      else
        fprintf(f, "Subject: Mail delivery failed%s\n\n",
          to_sender? ": returning message to sender" : "");

      /* output human readable part as text/plain section */
      fprintf(f, "--%s\n"
	  "Content-type: text/plain; charset=us-ascii\n\n",
	bound);

      if ((emf_text = next_emf(emf, US"intro")))
	fprintf(f, "%s", CS emf_text);
      else
        {
        fprintf(f,
/* This message has been reworded several times. It seems to be confusing to
somebody, however it is worded. I have retreated to the original, simple
wording. */
"This message was created automatically by mail delivery software.\n");

        if (bounce_message_text)
	  fprintf(f, "%s", CS bounce_message_text);
        if (to_sender)
          fprintf(f,
"\nA message that you sent could not be delivered to one or more of its\n"
"recipients. This is a permanent error. The following address(es) failed:\n");
        else
          fprintf(f,
"\nA message sent by\n\n  <%s>\n\n"
"could not be delivered to one or more of its recipients. The following\n"
"address(es) failed:\n", sender_address);
        }
      fputc('\n', f);

      /* Process the addresses, leaving them on the msgchain if they have a
      file name for a return message. (There has already been a check in
      post_process_one() for the existence of data in the message file.) A TRUE
      return from print_address_information() means that the address is not
      hidden. */

      paddr = &msgchain;
      for (addr = msgchain; addr; addr = *paddr)
        {
        if (print_address_information(addr, f, US"  ", US"\n    ", US""))
          print_address_error(addr, f, US"");

        /* End the final line for the address */

        fputc('\n', f);

        /* Leave on msgchain if there's a return file. */

        if (addr->return_file >= 0)
          {
          paddr = &(addr->next);
          filecount++;
          }

        /* Else save so that we can tick off the recipient when the
        message is sent. */

        else
          {
          *paddr = addr->next;
          addr->next = handled_addr;
          handled_addr = addr;
          }
        }

      fputc('\n', f);

      /* Get the next text, whether we need it or not, so as to be
      positioned for the one after. */

      emf_text = next_emf(emf, US"generated text");

      /* If there were any file messages passed by the local transports,
      include them in the message. Then put the address on the handled chain.
      In the case of a batch of addresses that were all sent to the same
      transport, the return_file field in all of them will contain the same
      fd, and the return_filename field in the *last* one will be set (to the
      name of the file). */

      if (msgchain)
        {
        address_item *nextaddr;

        if (emf_text)
	  fprintf(f, "%s", CS emf_text);
	else
          fprintf(f,
            "The following text was generated during the delivery "
            "attempt%s:\n", (filecount > 1)? "s" : "");

        for (addr = msgchain; addr; addr = nextaddr)
          {
          FILE *fm;
          address_item *topaddr = addr;

          /* List all the addresses that relate to this file */

	  fputc('\n', f);
          while(addr)                   /* Insurance */
            {
            print_address_information(addr, f, US"------ ",  US"\n       ",
              US" ------\n");
            if (addr->return_filename) break;
            addr = addr->next;
            }
	  fputc('\n', f);

          /* Now copy the file */

          if (!(fm = Ufopen(addr->return_filename, "rb")))
            fprintf(f, "    +++ Exim error... failed to open text file: %s\n",
              strerror(errno));
          else
            {
            while ((ch = fgetc(fm)) != EOF) fputc(ch, f);
            (void)fclose(fm);
            }
          Uunlink(addr->return_filename);

          /* Can now add to handled chain, first fishing off the next
          address on the msgchain. */

          nextaddr = addr->next;
          addr->next = handled_addr;
          handled_addr = topaddr;
          }
	fputc('\n', f);
        }

      /* output machine readable part */
#ifdef SUPPORT_I18N
      if (message_smtputf8)
	fprintf(f, "--%s\n"
	    "Content-type: message/global-delivery-status\n\n"
	    "Reporting-MTA: dns; %s\n",
	  bound, smtp_active_hostname);
      else
#endif
	fprintf(f, "--%s\n"
	    "Content-type: message/delivery-status\n\n"
	    "Reporting-MTA: dns; %s\n",
	  bound, smtp_active_hostname);

      if (dsn_envid)
	{
        /* must be decoded from xtext: see RFC 3461:6.3a */
        uschar *xdec_envid;
        if (auth_xtextdecode(dsn_envid, &xdec_envid) > 0)
          fprintf(f, "Original-Envelope-ID: %s\n", dsn_envid);
        else
          fprintf(f, "X-Original-Envelope-ID: error decoding xtext formatted ENVID\n");
        }
      fputc('\n', f);

      for (addr = handled_addr; addr; addr = addr->next)
        {
	host_item * hu;
        fprintf(f, "Action: failed\n"
	    "Final-Recipient: rfc822;%s\n"
	    "Status: 5.0.0\n",
	    addr->address);
        if ((hu = addr->host_used) && hu->name)
	  {
	  const uschar * s;
	  fprintf(f, "Remote-MTA: dns; %s\n", hu->name);
#ifdef EXPERIMENTAL_DSN_INFO
	  if (hu->address)
	    {
	    uschar * p = hu->port == 25
	      ? US"" : string_sprintf(":%d", hu->port);
	    fprintf(f, "Remote-MTA: X-ip; [%s]%s\n", hu->address, p);
	    }
	  if ((s = addr->smtp_greeting) && *s)
	    fprintf(f, "X-Remote-MTA-smtp-greeting: X-str; %s\n", s);
	  if ((s = addr->helo_response) && *s)
	    fprintf(f, "X-Remote-MTA-helo-response: X-str; %s\n", s);
	  if ((s = addr->message) && *s)
	    fprintf(f, "X-Exim-Diagnostic: X-str; %s\n", s);
#endif
	  print_dsn_diagnostic_code(addr, f);
	  }
	fputc('\n', f);
        }

      /* Now copy the message, trying to give an intelligible comment if
      it is too long for it all to be copied. The limit isn't strictly
      applied because of the buffering. There is, however, an option
      to suppress copying altogether. */

      emf_text = next_emf(emf, US"copy");

      /* add message body
         we ignore the intro text from template and add
         the text for bounce_return_size_limit at the end.

         bounce_return_message is ignored
         in case RET= is defined we honor these values
         otherwise bounce_return_body is honored.

         bounce_return_size_limit is always honored.
      */

      fprintf(f, "--%s\n", bound);

      dsnlimitmsg = US"X-Exim-DSN-Information: Due to administrative limits only headers are returned";
      dsnnotifyhdr = NULL;
      topt = topt_add_return_path;

      /* RET=HDRS? top priority */
      if (dsn_ret == dsn_ret_hdrs)
        topt |= topt_no_body;
      else
	{
	struct stat statbuf;

        /* no full body return at all? */
        if (!bounce_return_body)
          {
          topt |= topt_no_body;
          /* add header if we overrule RET=FULL */
          if (dsn_ret == dsn_ret_full)
            dsnnotifyhdr = dsnlimitmsg;
          }
	/* line length limited... return headers only if oversize */
        /* size limited ... return headers only if limit reached */
	else if (  max_received_linelength > bounce_return_linesize_limit
		|| (  bounce_return_size_limit > 0
		   && fstat(deliver_datafile, &statbuf) == 0
		   && statbuf.st_size > max
		)  )
	  {
	  topt |= topt_no_body;
	  dsnnotifyhdr = dsnlimitmsg;
          }
	}

#ifdef SUPPORT_I18N
      if (message_smtputf8)
	fputs(topt & topt_no_body ? "Content-type: message/global-headers\n\n"
				  : "Content-type: message/global\n\n",
	      f);
      else
#endif
	fputs(topt & topt_no_body ? "Content-type: text/rfc822-headers\n\n"
				  : "Content-type: message/rfc822\n\n",
	      f);

      fflush(f);
      transport_filter_argv = NULL;   /* Just in case */
      return_path = sender_address;   /* In case not previously set */
	{			      /* Dummy transport for headers add */
	transport_ctx tctx = {{0}};
	transport_instance tb = {0};

	tctx.u.fd = fileno(f);
	tctx.tblock = &tb;
	tctx.options = topt;
	tb.add_headers = dsnnotifyhdr;

	transport_write_message(&tctx, 0);
	}
      fflush(f);

      /* we never add the final text. close the file */
      if (emf)
        (void)fclose(emf);

      fprintf(f, "\n--%s--\n", bound);

      /* Close the file, which should send an EOF to the child process
      that is receiving the message. Wait for it to finish. */

      (void)fclose(f);
      rc = child_close(pid, 0);     /* Waits for child to close, no timeout */

      /* In the test harness, let the child do it's thing first. */

      if (running_in_test_harness) millisleep(500);

      /* If the process failed, there was some disaster in setting up the
      error message. Unless the message is very old, ensure that addr_defer
      is non-null, which will have the effect of leaving the message on the
      spool. The failed addresses will get tried again next time. However, we
      don't really want this to happen too often, so freeze the message unless
      there are some genuine deferred addresses to try. To do this we have
      to call spool_write_header() here, because with no genuine deferred
      addresses the normal code below doesn't get run. */

      if (rc != 0)
        {
        uschar *s = US"";
        if (now - received_time.tv_sec < retry_maximum_timeout && !addr_defer)
          {
          addr_defer = (address_item *)(+1);
          deliver_freeze = TRUE;
          deliver_frozen_at = time(NULL);
          /* Panic-dies on error */
          (void)spool_write_header(message_id, SW_DELIVERING, NULL);
          s = US" (frozen)";
          }
        deliver_msglog("Process failed (%d) when writing error message "
          "to %s%s", rc, bounce_recipient, s);
        log_write(0, LOG_MAIN, "Process failed (%d) when writing error message "
          "to %s%s", rc, bounce_recipient, s);
        }

      /* The message succeeded. Ensure that the recipients that failed are
      now marked finished with on the spool and their parents updated. */

      else
        {
        for (addr = handled_addr; addr; addr = addr->next)
          {
          address_done(addr, logtod);
          child_done(addr, logtod);
          }
        /* Panic-dies on error */
        (void)spool_write_header(message_id, SW_DELIVERING, NULL);
        }
      }
    }
  }

disable_logging = FALSE;  /* In case left set */

/* Come here from the mua_wrapper case if routing goes wrong */

DELIVERY_TIDYUP:

/* If there are now no deferred addresses, we are done. Preserve the
message log if so configured, and we are using them. Otherwise, sling it.
Then delete the message itself. */

if (!addr_defer)
  {
  uschar * fname;

  if (message_logs)
    {
    fname = spool_fname(US"msglog", message_subdir, id, US"");
    if (preserve_message_logs)
      {
      int rc;
      uschar * moname = spool_fname(US"msglog.OLD", US"", id, US"");

      if ((rc = Urename(fname, moname)) < 0)
        {
        (void)directory_make(spool_directory,
			      spool_sname(US"msglog.OLD", US""),
			      MSGLOG_DIRECTORY_MODE, TRUE);
        rc = Urename(fname, moname);
        }
      if (rc < 0)
        log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to move %s to the "
          "msglog.OLD directory", fname);
      }
    else
      if (Uunlink(fname) < 0)
        log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to unlink %s: %s",
		  fname, strerror(errno));
    }

  /* Remove the two message files. */

  fname = spool_fname(US"input", message_subdir, id, US"-D");
  if (Uunlink(fname) < 0)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to unlink %s: %s",
      fname, strerror(errno));
  fname = spool_fname(US"input", message_subdir, id, US"-H");
  if (Uunlink(fname) < 0)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to unlink %s: %s",
      fname, strerror(errno));

  /* Log the end of this message, with queue time if requested. */

  if (LOGGING(queue_time_overall))
    log_write(0, LOG_MAIN, "Completed QT=%s", string_timesince(&received_time));
  else
    log_write(0, LOG_MAIN, "Completed");

  /* Unset deliver_freeze so that we won't try to move the spool files further down */
  deliver_freeze = FALSE;

#ifndef DISABLE_EVENT
  (void) event_raise(event_action, US"msg:complete", NULL);
#endif
  }

/* If there are deferred addresses, we are keeping this message because it is
not yet completed. Lose any temporary files that were catching output from
pipes for any of the deferred addresses, handle one-time aliases, and see if
the message has been on the queue for so long that it is time to send a warning
message to the sender, unless it is a mailer-daemon. If all deferred addresses
have the same domain, we can set deliver_domain for the expansion of
delay_warning_ condition - if any of them are pipes, files, or autoreplies, use
the parent's domain.

If all the deferred addresses have an error number that indicates "retry time
not reached", skip sending the warning message, because it won't contain the
reason for the delay. It will get sent at the next real delivery attempt.
However, if at least one address has tried, we'd better include all of them in
the message.

If we can't make a process to send the message, don't worry.

For mailing list expansions we want to send the warning message to the
mailing list manager. We can't do a perfect job here, as some addresses may
have different errors addresses, but if we take the errors address from
each deferred address it will probably be right in most cases.

If addr_defer == +1, it means there was a problem sending an error message
for failed addresses, and there were no "real" deferred addresses. The value
was set just to keep the message on the spool, so there is nothing to do here.
*/

else if (addr_defer != (address_item *)(+1))
  {
  address_item *addr;
  uschar *recipients = US"";
  BOOL delivery_attempted = FALSE;

  deliver_domain = testflag(addr_defer, af_pfr)
    ? addr_defer->parent->domain : addr_defer->domain;

  for (addr = addr_defer; addr; addr = addr->next)
    {
    address_item *otaddr;

    if (addr->basic_errno > ERRNO_RETRY_BASE) delivery_attempted = TRUE;

    if (deliver_domain)
      {
      const uschar *d = testflag(addr, af_pfr)
	? addr->parent->domain : addr->domain;

      /* The domain may be unset for an address that has never been routed
      because the system filter froze the message. */

      if (!d || Ustrcmp(d, deliver_domain) != 0)
        deliver_domain = NULL;
      }

    if (addr->return_filename) Uunlink(addr->return_filename);

    /* Handle the case of one-time aliases. If any address in the ancestry
    of this one is flagged, ensure it is in the recipients list, suitably
    flagged, and that its parent is marked delivered. */

    for (otaddr = addr; otaddr; otaddr = otaddr->parent)
      if (otaddr->onetime_parent) break;

    if (otaddr)
      {
      int i;
      int t = recipients_count;

      for (i = 0; i < recipients_count; i++)
        {
        uschar *r = recipients_list[i].address;
        if (Ustrcmp(otaddr->onetime_parent, r) == 0) t = i;
        if (Ustrcmp(otaddr->address, r) == 0) break;
        }

      /* Didn't find the address already in the list, and did find the
      ultimate parent's address in the list, and they really are different
      (i.e. not from an identity-redirect). After adding the recipient,
      update the errors address in the recipients list. */

      if (  i >= recipients_count && t < recipients_count
         && Ustrcmp(otaddr->address, otaddr->parent->address) != 0)
        {
        DEBUG(D_deliver) debug_printf("one_time: adding %s in place of %s\n",
          otaddr->address, otaddr->parent->address);
        receive_add_recipient(otaddr->address, t);
        recipients_list[recipients_count-1].errors_to = otaddr->prop.errors_address;
        tree_add_nonrecipient(otaddr->parent->address);
        update_spool = TRUE;
        }
      }

    /* Except for error messages, ensure that either the errors address for
    this deferred address or, if there is none, the sender address, is on the
    list of recipients for a warning message. */

    if (sender_address[0])
      {
      uschar * s = addr->prop.errors_address;
      if (!s) s = sender_address;
      if (Ustrstr(recipients, s) == NULL)
	recipients = string_sprintf("%s%s%s", recipients,
	  recipients[0] ? "," : "", s);
      }
    }

  /* Send a warning message if the conditions are right. If the condition check
  fails because of a lookup defer, there is nothing we can do. The warning
  is not sent. Another attempt will be made at the next delivery attempt (if
  it also defers). */

  if (  !queue_2stage
     && delivery_attempted
     && (  ((addr_defer->dsn_flags & rf_dsnflags) == 0)
        || (addr_defer->dsn_flags & rf_notify_delay) == rf_notify_delay
	)
     && delay_warning[1] > 0
     && sender_address[0] != 0
     && (  !delay_warning_condition
        || expand_check_condition(delay_warning_condition,
            US"delay_warning", US"option")
	)
     )
    {
    int count;
    int show_time;
    int queue_time = time(NULL) - received_time.tv_sec;

    /* When running in the test harness, there's an option that allows us to
    fudge this time so as to get repeatability of the tests. Take the first
    time off the list. In queue runs, the list pointer gets updated in the
    calling process. */

    if (running_in_test_harness && fudged_queue_times[0] != 0)
      {
      int qt = readconf_readtime(fudged_queue_times, '/', FALSE);
      if (qt >= 0)
        {
        DEBUG(D_deliver) debug_printf("fudged queue_times = %s\n",
          fudged_queue_times);
        queue_time = qt;
        }
      }

    /* See how many warnings we should have sent by now */

    for (count = 0; count < delay_warning[1]; count++)
      if (queue_time < delay_warning[count+2]) break;

    show_time = delay_warning[count+1];

    if (count >= delay_warning[1])
      {
      int extra;
      int last_gap = show_time;
      if (count > 1) last_gap -= delay_warning[count];
      extra = (queue_time - delay_warning[count+1])/last_gap;
      show_time += last_gap * extra;
      count += extra;
      }

    DEBUG(D_deliver)
      {
      debug_printf("time on queue = %s\n", readconf_printtime(queue_time));
      debug_printf("warning counts: required %d done %d\n", count,
        warning_count);
      }

    /* We have computed the number of warnings there should have been by now.
    If there haven't been enough, send one, and up the count to what it should
    have been. */

    if (warning_count < count)
      {
      header_line *h;
      int fd;
      pid_t pid = child_open_exim(&fd);

      if (pid > 0)
        {
        uschar *wmf_text;
        FILE *wmf = NULL;
        FILE *f = fdopen(fd, "wb");
	uschar * bound;
	transport_ctx tctx = {{0}};

        if (warn_message_file)
          if (!(wmf = Ufopen(warn_message_file, "rb")))
            log_write(0, LOG_MAIN|LOG_PANIC, "Failed to open %s for warning "
              "message texts: %s", warn_message_file, strerror(errno));

        warnmsg_recipients = recipients;
        warnmsg_delay = queue_time < 120*60
	  ? string_sprintf("%d minutes", show_time/60)
	  : string_sprintf("%d hours", show_time/3600);

        if (errors_reply_to)
          fprintf(f, "Reply-To: %s\n", errors_reply_to);
        fprintf(f, "Auto-Submitted: auto-replied\n");
        moan_write_from(f);
        fprintf(f, "To: %s\n", recipients);

        /* generated boundary string and output MIME-Headers */
        bound = string_sprintf(TIME_T_FMT "-eximdsn-%d", time(NULL), rand());

        fprintf(f, "Content-Type: multipart/report;"
	    " report-type=delivery-status; boundary=%s\n"
	    "MIME-Version: 1.0\n",
	  bound);

        if ((wmf_text = next_emf(wmf, US"header")))
          fprintf(f, "%s\n", wmf_text);
        else
          fprintf(f, "Subject: Warning: message %s delayed %s\n\n",
            message_id, warnmsg_delay);

        /* output human readable part as text/plain section */
        fprintf(f, "--%s\n"
	    "Content-type: text/plain; charset=us-ascii\n\n",
	  bound);

        if ((wmf_text = next_emf(wmf, US"intro")))
	  fprintf(f, "%s", CS wmf_text);
	else
          {
          fprintf(f,
"This message was created automatically by mail delivery software.\n");

          if (Ustrcmp(recipients, sender_address) == 0)
            fprintf(f,
"A message that you sent has not yet been delivered to one or more of its\n"
"recipients after more than ");

          else
	    fprintf(f,
"A message sent by\n\n  <%s>\n\n"
"has not yet been delivered to one or more of its recipients after more than \n",
	      sender_address);

          fprintf(f, "%s on the queue on %s.\n\n"
	      "The message identifier is:     %s\n",
	    warnmsg_delay, primary_hostname, message_id);

          for (h = header_list; h; h = h->next)
            if (strncmpic(h->text, US"Subject:", 8) == 0)
              fprintf(f, "The subject of the message is: %s", h->text + 9);
            else if (strncmpic(h->text, US"Date:", 5) == 0)
              fprintf(f, "The date of the message is:    %s", h->text + 6);
          fputc('\n', f);

          fprintf(f, "The address%s to which the message has not yet been "
            "delivered %s:\n",
            !addr_defer->next ? "" : "es",
            !addr_defer->next ? "is": "are");
          }

        /* List the addresses, with error information if allowed */

        /* store addr_defer for machine readable part */
        address_item *addr_dsndefer = addr_defer;
        fputc('\n', f);
        while (addr_defer)
          {
          address_item *addr = addr_defer;
          addr_defer = addr->next;
          if (print_address_information(addr, f, US"  ", US"\n    ", US""))
            print_address_error(addr, f, US"Delay reason: ");
          fputc('\n', f);
          }
        fputc('\n', f);

        /* Final text */

        if (wmf)
          {
          if ((wmf_text = next_emf(wmf, US"final")))
	    fprintf(f, "%s", CS wmf_text);
          (void)fclose(wmf);
          }
        else
          {
          fprintf(f,
"No action is required on your part. Delivery attempts will continue for\n"
"some time, and this warning may be repeated at intervals if the message\n"
"remains undelivered. Eventually the mail delivery software will give up,\n"
"and when that happens, the message will be returned to you.\n");
          }

        /* output machine readable part */
        fprintf(f, "\n--%s\n"
	    "Content-type: message/delivery-status\n\n"
	    "Reporting-MTA: dns; %s\n",
	  bound,
	  smtp_active_hostname);


        if (dsn_envid)
	  {
          /* must be decoded from xtext: see RFC 3461:6.3a */
          uschar *xdec_envid;
          if (auth_xtextdecode(dsn_envid, &xdec_envid) > 0)
            fprintf(f,"Original-Envelope-ID: %s\n", dsn_envid);
          else
            fprintf(f,"X-Original-Envelope-ID: error decoding xtext formatted ENVID\n");
          }
        fputc('\n', f);

        for ( ; addr_dsndefer; addr_dsndefer = addr_dsndefer->next)
          {
          if (addr_dsndefer->dsn_orcpt)
            fprintf(f, "Original-Recipient: %s\n", addr_dsndefer->dsn_orcpt);

          fprintf(f, "Action: delayed\n"
	      "Final-Recipient: rfc822;%s\n"
	      "Status: 4.0.0\n",
	    addr_dsndefer->address);
          if (addr_dsndefer->host_used && addr_dsndefer->host_used->name)
            {
            fprintf(f, "Remote-MTA: dns; %s\n",
		    addr_dsndefer->host_used->name);
            print_dsn_diagnostic_code(addr_dsndefer, f);
            }
	  fputc('\n', f);
          }

        fprintf(f, "--%s\n"
	    "Content-type: text/rfc822-headers\n\n",
	  bound);

        fflush(f);
        /* header only as required by RFC. only failure DSN needs to honor RET=FULL */
	tctx.u.fd = fileno(f);
        tctx.options = topt_add_return_path | topt_no_body;
        transport_filter_argv = NULL;   /* Just in case */
        return_path = sender_address;   /* In case not previously set */

        /* Write the original email out */
        transport_write_message(&tctx, 0);
        fflush(f);

        fprintf(f,"\n--%s--\n", bound);

        fflush(f);

        /* Close and wait for child process to complete, without a timeout.
        If there's an error, don't update the count. */

        (void)fclose(f);
        if (child_close(pid, 0) == 0)
          {
          warning_count = count;
          update_spool = TRUE;    /* Ensure spool rewritten */
          }
        }
      }
    }

  /* Clear deliver_domain */

  deliver_domain = NULL;

  /* If this was a first delivery attempt, unset the first time flag, and
  ensure that the spool gets updated. */

  if (deliver_firsttime)
    {
    deliver_firsttime = FALSE;
    update_spool = TRUE;
    }

  /* If delivery was frozen and freeze_tell is set, generate an appropriate
  message, unless the message is a local error message (to avoid loops). Then
  log the freezing. If the text in "frozen_info" came from a system filter,
  it has been escaped into printing characters so as not to mess up log lines.
  For the "tell" message, we turn \n back into newline. Also, insert a newline
  near the start instead of the ": " string. */

  if (deliver_freeze)
    {
    if (freeze_tell && freeze_tell[0] != 0 && !local_error_message)
      {
      uschar *s = string_copy(frozen_info);
      uschar *ss = Ustrstr(s, " by the system filter: ");

      if (ss != NULL)
        {
        ss[21] = '.';
        ss[22] = '\n';
        }

      ss = s;
      while (*ss != 0)
        {
        if (*ss == '\\' && ss[1] == 'n')
          {
          *ss++ = ' ';
          *ss++ = '\n';
          }
        else ss++;
        }
      moan_tell_someone(freeze_tell, addr_defer, US"Message frozen",
        "Message %s has been frozen%s.\nThe sender is <%s>.\n", message_id,
        s, sender_address);
      }

    /* Log freezing just before we update the -H file, to minimize the chance
    of a race problem. */

    deliver_msglog("*** Frozen%s\n", frozen_info);
    log_write(0, LOG_MAIN, "Frozen%s", frozen_info);
    }

  /* If there have been any updates to the non-recipients list, or other things
  that get written to the spool, we must now update the spool header file so
  that it has the right information for the next delivery attempt. If there
  was more than one address being delivered, the header_change update is done
  earlier, in case one succeeds and then something crashes. */

  DEBUG(D_deliver)
    debug_printf("delivery deferred: update_spool=%d header_rewritten=%d\n",
      update_spool, header_rewritten);

  if (update_spool || header_rewritten)
    /* Panic-dies on error */
    (void)spool_write_header(message_id, SW_DELIVERING, NULL);
  }

/* Finished with the message log. If the message is complete, it will have
been unlinked or renamed above. */

if (message_logs) (void)fclose(message_log);

/* Now we can close and remove the journal file. Its only purpose is to record
successfully completed deliveries asap so that this information doesn't get
lost if Exim (or the machine) crashes. Forgetting about a failed delivery is
not serious, as trying it again is not harmful. The journal might not be open
if all addresses were deferred at routing or directing. Nevertheless, we must
remove it if it exists (may have been lying around from a crash during the
previous delivery attempt). We don't remove the journal if a delivery
subprocess failed to pass back delivery information; this is controlled by
the remove_journal flag. When the journal is left, we also don't move the
message off the main spool if frozen and the option is set. It should get moved
at the next attempt, after the journal has been inspected. */

if (journal_fd >= 0) (void)close(journal_fd);

if (remove_journal)
  {
  uschar * fname = spool_fname(US"input", message_subdir, id, US"-J");

  if (Uunlink(fname) < 0 && errno != ENOENT)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to unlink %s: %s", fname,
      strerror(errno));

  /* Move the message off the spool if requested */

#ifdef SUPPORT_MOVE_FROZEN_MESSAGES
  if (deliver_freeze && move_frozen_messages)
    (void)spool_move_message(id, message_subdir, US"", US"F");
#endif
  }

/* Closing the data file frees the lock; if the file has been unlinked it
will go away. Otherwise the message becomes available for another process
to try delivery. */

(void)close(deliver_datafile);
deliver_datafile = -1;
DEBUG(D_deliver) debug_printf("end delivery of %s\n", id);

/* It is unlikely that there will be any cached resources, since they are
released after routing, and in the delivery subprocesses. However, it's
possible for an expansion for something afterwards (for example,
expand_check_condition) to do a lookup. We must therefore be sure everything is
released. */

search_tidyup();
acl_where = ACL_WHERE_UNKNOWN;
return final_yield;
}