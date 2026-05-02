receive_msg(BOOL extract_recip)
{
int  i;
int  rc = FAIL;
int  msg_size = 0;
int  process_info_len = Ustrlen(process_info);
int  error_rc = (error_handling == ERRORS_SENDER)?
       errors_sender_rc : EXIT_FAILURE;
int  header_size = 256;
int  start, end, domain;
int  id_resolution;
int  had_zero = 0;
int  prevlines_length = 0;

register int ptr = 0;

BOOL contains_resent_headers = FALSE;
BOOL extracted_ignored = FALSE;
BOOL first_line_ended_crlf = TRUE_UNSET;
BOOL smtp_yield = TRUE;
BOOL yield = FALSE;

BOOL resents_exist = FALSE;
uschar *resent_prefix = US"";
uschar *blackholed_by = NULL;
uschar *blackhole_log_msg = US"";
enum {NOT_TRIED, TMP_REJ, PERM_REJ, ACCEPTED} cutthrough_done = NOT_TRIED;

flock_t lock_data;
error_block *bad_addresses = NULL;

uschar *frozen_by = NULL;
uschar *queued_by = NULL;

uschar *errmsg;
gstring * g;
struct stat statbuf;

/* Final message to give to SMTP caller, and messages from ACLs */

uschar *smtp_reply = NULL;
uschar *user_msg, *log_msg;

/* Working header pointers */

header_line *h, *next;

/* Flags for noting the existence of certain headers (only one left) */

BOOL date_header_exists = FALSE;

/* Pointers to receive the addresses of headers whose contents we need. */

header_line *from_header = NULL;
header_line *subject_header = NULL;
header_line *msgid_header = NULL;
header_line *received_header;

#ifdef EXPERIMENTAL_DMARC
int dmarc_up = 0;
#endif /* EXPERIMENTAL_DMARC */

/* Variables for use when building the Received: header. */

uschar *timestamp;
int tslen;

/* Release any open files that might have been cached while preparing to
accept the message - e.g. by verifying addresses - because reading a message
might take a fair bit of real time. */

search_tidyup();

/* Extracting the recipient list from an input file is incompatible with
cutthrough delivery with the no-spool option.  It shouldn't be possible
to set up the combination, but just in case kill any ongoing connection. */
if (extract_recip || !smtp_input)
  cancel_cutthrough_connection(TRUE, US"not smtp input");

/* Initialize the chain of headers by setting up a place-holder for Received:
header. Temporarily mark it as "old", i.e. not to be used. We keep header_last
pointing to the end of the chain to make adding headers simple. */

received_header = header_list = header_last = store_get(sizeof(header_line));
header_list->next = NULL;
header_list->type = htype_old;
header_list->text = NULL;
header_list->slen = 0;

/* Control block for the next header to be read. */

next = store_get(sizeof(header_line));
next->text = store_get(header_size);

/* Initialize message id to be null (indicating no message read), and the
header names list to be the normal list. Indicate there is no data file open
yet, initialize the size and warning count, and deal with no size limit. */

message_id[0] = 0;
data_file = NULL;
data_fd = -1;
spool_name = US"";
message_size = 0;
warning_count = 0;
received_count = 1;            /* For the one we will add */

if (thismessage_size_limit <= 0) thismessage_size_limit = INT_MAX;

/* While reading the message, the following counts are computed. */

message_linecount = body_linecount = body_zerocount =
  max_received_linelength = 0;

#ifndef DISABLE_DKIM
/* Call into DKIM to set up the context.  In CHUNKING mode
we clear the dot-stuffing flag */
if (smtp_input && !smtp_batched_input && !dkim_disable_verify)
  dkim_exim_verify_init(chunking_state <= CHUNKING_OFFERED);
#endif

#ifdef EXPERIMENTAL_DMARC
/* initialize libopendmarc */
dmarc_up = dmarc_init();
#endif

/* Remember the time of reception. Exim uses time+pid for uniqueness of message
ids, and fractions of a second are required. See the comments that precede the
message id creation below. */

(void)gettimeofday(&message_id_tv, NULL);

/* For other uses of the received time we can operate with granularity of one
second, and for that we use the global variable received_time. This is for
things like ultimate message timeouts.XXX */

received_time = message_id_tv;

/* If SMTP input, set the special handler for timeouts. The alarm() calls
happen in the smtp_getc() function when it refills its buffer. */

if (smtp_input) os_non_restarting_signal(SIGALRM, data_timeout_handler);

/* If not SMTP input, timeout happens only if configured, and we just set a
single timeout for the whole message. */

else if (receive_timeout > 0)
  {
  os_non_restarting_signal(SIGALRM, data_timeout_handler);
  alarm(receive_timeout);
  }

/* SIGTERM and SIGINT are caught always. */

signal(SIGTERM, data_sigterm_sigint_handler);
signal(SIGINT, data_sigterm_sigint_handler);

/* Header lines in messages are not supposed to be very long, though when
unfolded, to: and cc: headers can take up a lot of store. We must also cope
with the possibility of junk being thrown at us. Start by getting 256 bytes for
storing the header, and extend this as necessary using string_cat().

To cope with total lunacies, impose an upper limit on the length of the header
section of the message, as otherwise the store will fill up. We must also cope
with the possibility of binary zeros in the data. Hence we cannot use fgets().
Folded header lines are joined into one string, leaving the '\n' characters
inside them, so that writing them out reproduces the input.

Loop for each character of each header; the next structure for chaining the
header is set up already, with ptr the offset of the next character in
next->text. */

for (;;)
  {
  int ch = (receive_getc)(GETC_BUFFER_UNLIMITED);

  /* If we hit EOF on a SMTP connection, it's an error, since incoming
  SMTP must have a correct "." terminator. */

  if (ch == EOF && smtp_input /* && !smtp_batched_input */)
    {
    smtp_reply = handle_lost_connection(US" (header)");
    smtp_yield = FALSE;
    goto TIDYUP;                       /* Skip to end of function */
    }

  /* See if we are at the current header's size limit - there must be at least
  four bytes left. This allows for the new character plus a zero, plus two for
  extra insertions when we are playing games with dots and carriage returns. If
  we are at the limit, extend the text buffer. This could have been done
  automatically using string_cat() but because this is a tightish loop storing
  only one character at a time, we choose to do it inline. Normally
  store_extend() will be able to extend the block; only at the end of a big
  store block will a copy be needed. To handle the case of very long headers
  (and sometimes lunatic messages can have ones that are 100s of K long) we
  call store_release() for strings that have been copied - if the string is at
  the start of a block (and therefore the only thing in it, because we aren't
  doing any other gets), the block gets freed. We can only do this release if
  there were no allocations since the once that we want to free. */

  if (ptr >= header_size - 4)
    {
    int oldsize = header_size;
    /* header_size += 256; */
    header_size *= 2;
    if (!store_extend(next->text, oldsize, header_size))
      {
      BOOL release_ok = store_last_get[store_pool] == next->text;
      uschar *newtext = store_get(header_size);
      memcpy(newtext, next->text, ptr);
      if (release_ok) store_release(next->text);
      next->text = newtext;
      }
    }

  /* Cope with receiving a binary zero. There is dispute about whether
  these should be allowed in RFC 822 messages. The middle view is that they
  should not be allowed in headers, at least. Exim takes this attitude at
  the moment. We can't just stomp on them here, because we don't know that
  this line is a header yet. Set a flag to cause scanning later. */

  if (ch == 0) had_zero++;

  /* Test for termination. Lines in remote SMTP are terminated by CRLF, while
  those from data files use just LF. Treat LF in local SMTP input as a
  terminator too. Treat EOF as a line terminator always. */

  if (ch == EOF) goto EOL;

  /* FUDGE: There are sites out there that don't send CRs before their LFs, and
  other MTAs accept this. We are therefore forced into this "liberalisation"
  too, so we accept LF as a line terminator whatever the source of the message.
  However, if the first line of the message ended with a CRLF, we treat a bare
  LF specially by inserting a white space after it to ensure that the header
  line is not terminated. */

  if (ch == '\n')
    {
    if (first_line_ended_crlf == TRUE_UNSET) first_line_ended_crlf = FALSE;
      else if (first_line_ended_crlf) receive_ungetc(' ');
    goto EOL;
    }

  /* This is not the end of the line. If this is SMTP input and this is
  the first character in the line and it is a "." character, ignore it.
  This implements the dot-doubling rule, though header lines starting with
  dots aren't exactly common. They are legal in RFC 822, though. If the
  following is CRLF or LF, this is the line that that terminates the
  entire message. We set message_ended to indicate this has happened (to
  prevent further reading), and break out of the loop, having freed the
  empty header, and set next = NULL to indicate no data line. */

  if (ptr == 0 && ch == '.' && (smtp_input || dot_ends))
    {
    ch = (receive_getc)(GETC_BUFFER_UNLIMITED);
    if (ch == '\r')
      {
      ch = (receive_getc)(GETC_BUFFER_UNLIMITED);
      if (ch != '\n')
        {
        receive_ungetc(ch);
        ch = '\r';              /* Revert to CR */
        }
      }
    if (ch == '\n')
      {
      message_ended = END_DOT;
      store_reset(next);
      next = NULL;
      break;                    /* End character-reading loop */
      }

    /* For non-SMTP input, the dot at the start of the line was really a data
    character. What is now in ch is the following character. We guaranteed
    enough space for this above. */

    if (!smtp_input)
      {
      next->text[ptr++] = '.';
      message_size++;
      }
    }

  /* If CR is immediately followed by LF, end the line, ignoring the CR, and
  remember this case if this is the first line ending. */

  if (ch == '\r')
    {
    ch = (receive_getc)(GETC_BUFFER_UNLIMITED);
    if (ch == '\n')
      {
      if (first_line_ended_crlf == TRUE_UNSET) first_line_ended_crlf = TRUE;
      goto EOL;
      }

    /* Otherwise, put back the character after CR, and turn the bare CR
    into LF SP. */

    ch = (receive_ungetc)(ch);
    next->text[ptr++] = '\n';
    message_size++;
    ch = ' ';
    }

  /* We have a data character for the header line. */

  next->text[ptr++] = ch;    /* Add to buffer */
  message_size++;            /* Total message size so far */

  /* Handle failure due to a humungously long header section. The >= allows
  for the terminating \n. Add what we have so far onto the headers list so
  that it gets reflected in any error message, and back up the just-read
  character. */

  if (message_size >= header_maxsize)
    {
    next->text[ptr] = 0;
    next->slen = ptr;
    next->type = htype_other;
    next->next = NULL;
    header_last->next = next;
    header_last = next;

    log_write(0, LOG_MAIN, "ridiculously long message header received from "
      "%s (more than %d characters): message abandoned",
      sender_host_unknown? sender_ident : sender_fullhost, header_maxsize);

    if (smtp_input)
      {
      smtp_reply = US"552 Message header is ridiculously long";
      receive_swallow_smtp();
      goto TIDYUP;                             /* Skip to end of function */
      }

    else
      {
      give_local_error(ERRMESS_VLONGHEADER,
        string_sprintf("message header longer than %d characters received: "
         "message not accepted", header_maxsize), US"", error_rc, stdin,
           header_list->next);
      /* Does not return */
      }
    }

  continue;                  /* With next input character */

  /* End of header line reached */

  EOL:

  /* Keep track of lines for BSMTP errors and overall message_linecount. */

  receive_linecount++;
  message_linecount++;

  /* Keep track of maximum line length */

  if (ptr - prevlines_length > max_received_linelength)
    max_received_linelength = ptr - prevlines_length;
  prevlines_length = ptr + 1;

  /* Now put in the terminating newline. There is always space for
  at least two more characters. */

  next->text[ptr++] = '\n';
  message_size++;

  /* A blank line signals the end of the headers; release the unwanted
  space and set next to NULL to indicate this. */

  if (ptr == 1)
    {
    store_reset(next);
    next = NULL;
    break;
    }

  /* There is data in the line; see if the next input character is a
  whitespace character. If it is, we have a continuation of this header line.
  There is always space for at least one character at this point. */

  if (ch != EOF)
    {
    int nextch = (receive_getc)(GETC_BUFFER_UNLIMITED);
    if (nextch == ' ' || nextch == '\t')
      {
      next->text[ptr++] = nextch;
      message_size++;
      continue;                      /* Iterate the loop */
      }
    else if (nextch != EOF) (receive_ungetc)(nextch);   /* For next time */
    else ch = EOF;                   /* Cause main loop to exit at end */
    }

  /* We have got to the real line end. Terminate the string and release store
  beyond it. If it turns out to be a real header, internal binary zeros will
  be squashed later. */

  next->text[ptr] = 0;
  next->slen = ptr;
  store_reset(next->text + ptr + 1);

  /* Check the running total size against the overall message size limit. We
  don't expect to fail here, but if the overall limit is set less than MESSAGE_
  MAXSIZE and a big header is sent, we want to catch it. Just stop reading
  headers - the code to read the body will then also hit the buffer. */

  if (message_size > thismessage_size_limit) break;

  /* A line that is not syntactically correct for a header also marks
  the end of the headers. In this case, we leave next containing the
  first data line. This might actually be several lines because of the
  continuation logic applied above, but that doesn't matter.

  It turns out that smail, and presumably sendmail, accept leading lines
  of the form

  From ph10 Fri Jan  5 12:35 GMT 1996

  in messages. The "mail" command on Solaris 2 sends such lines. I cannot
  find any documentation of this, but for compatibility it had better be
  accepted. Exim restricts it to the case of non-smtp messages, and
  treats it as an alternative to the -f command line option. Thus it is
  ignored except for trusted users or filter testing. Otherwise it is taken
  as the sender address, unless -f was used (sendmail compatibility).

  It further turns out that some UUCPs generate the From_line in a different
  format, e.g.

  From ph10 Fri, 7 Jan 97 14:00:00 GMT

  The regex for matching these things is now capable of recognizing both
  formats (including 2- and 4-digit years in the latter). In fact, the regex
  is now configurable, as is the expansion string to fish out the sender.

  Even further on it has been discovered that some broken clients send
  these lines in SMTP messages. There is now an option to ignore them from
  specified hosts or networks. Sigh. */

  if (header_last == header_list &&
       (!smtp_input
         ||
         (sender_host_address != NULL &&
           verify_check_host(&ignore_fromline_hosts) == OK)
         ||
         (sender_host_address == NULL && ignore_fromline_local)
       ) &&
       regex_match_and_setup(regex_From, next->text, 0, -1))
    {
    if (!sender_address_forced)
      {
      uschar *uucp_sender = expand_string(uucp_from_sender);
      if (uucp_sender == NULL)
        {
        log_write(0, LOG_MAIN|LOG_PANIC,
          "expansion of \"%s\" failed after matching "
          "\"From \" line: %s", uucp_from_sender, expand_string_message);
        }
      else
        {
        int start, end, domain;
        uschar *errmess;
        uschar *newsender = parse_extract_address(uucp_sender, &errmess,
          &start, &end, &domain, TRUE);
        if (newsender != NULL)
          {
          if (domain == 0 && newsender[0] != 0)
            newsender = rewrite_address_qualify(newsender, FALSE);

          if (filter_test != FTEST_NONE || receive_check_set_sender(newsender))
            {
            sender_address = newsender;

            if (trusted_caller || filter_test != FTEST_NONE)
              {
              authenticated_sender = NULL;
              originator_name = US"";
              sender_local = FALSE;
              }

            if (filter_test != FTEST_NONE)
              printf("Sender taken from \"From \" line\n");
            }
          }
        }
      }
    }

  /* Not a leading "From " line. Check to see if it is a valid header line.
  Header names may contain any non-control characters except space and colon,
  amazingly. */

  else
    {
    uschar *p = next->text;

    /* If not a valid header line, break from the header reading loop, leaving
    next != NULL, indicating that it holds the first line of the body. */

    if (isspace(*p)) break;
    while (mac_isgraph(*p) && *p != ':') p++;
    while (isspace(*p)) p++;
    if (*p != ':')
      {
      body_zerocount = had_zero;
      break;
      }

    /* We have a valid header line. If there were any binary zeroes in
    the line, stomp on them here. */

    if (had_zero > 0)
      for (p = next->text; p < next->text + ptr; p++) if (*p == 0) *p = '?';

    /* It is perfectly legal to have an empty continuation line
    at the end of a header, but it is confusing to humans
    looking at such messages, since it looks like a blank line.
    Reduce confusion by removing redundant white space at the
    end. We know that there is at least one printing character
    (the ':' tested for above) so there is no danger of running
    off the end. */

    p = next->text + ptr - 2;
    for (;;)
      {
      while (*p == ' ' || *p == '\t') p--;
      if (*p != '\n') break;
      ptr = (p--) - next->text + 1;
      message_size -= next->slen - ptr;
      next->text[ptr] = 0;
      next->slen = ptr;
      }

    /* Add the header to the chain */

    next->type = htype_other;
    next->next = NULL;
    header_last->next = next;
    header_last = next;

    /* Check the limit for individual line lengths. This comes after adding to
    the chain so that the failing line is reflected if a bounce is generated
    (for a local message). */

    if (header_line_maxsize > 0 && next->slen > header_line_maxsize)
      {
      log_write(0, LOG_MAIN, "overlong message header line received from "
        "%s (more than %d characters): message abandoned",
        sender_host_unknown? sender_ident : sender_fullhost,
        header_line_maxsize);

      if (smtp_input)
        {
        smtp_reply = US"552 A message header line is too long";
        receive_swallow_smtp();
        goto TIDYUP;                             /* Skip to end of function */
        }

      else
        {
        give_local_error(ERRMESS_VLONGHDRLINE,
          string_sprintf("message header line longer than %d characters "
           "received: message not accepted", header_line_maxsize), US"",
           error_rc, stdin, header_list->next);
        /* Does not return */
        }
      }

    /* Note if any resent- fields exist. */

    if (!resents_exist && strncmpic(next->text, US"resent-", 7) == 0)
      {
      resents_exist = TRUE;
      resent_prefix = US"Resent-";
      }
    }

  /* Reject CHUNKING messages that do not CRLF their first header line */

  if (!first_line_ended_crlf && chunking_state > CHUNKING_OFFERED)
    {
    log_write(L_size_reject, LOG_MAIN|LOG_REJECT, "rejected from <%s>%s%s%s%s: "
      "Non-CRLF-terminated header, under CHUNKING: message abandoned",
      sender_address,
      sender_fullhost ? " H=" : "", sender_fullhost ? sender_fullhost : US"",
      sender_ident ? " U=" : "",    sender_ident ? sender_ident : US"");
    smtp_printf("552 Message header not CRLF terminated\r\n", FALSE);
    bdat_flush_data();
    smtp_reply = US"";
    goto TIDYUP;                             /* Skip to end of function */
    }

  /* The line has been handled. If we have hit EOF, break out of the loop,
  indicating no pending data line. */

  if (ch == EOF) { next = NULL; break; }

  /* Set up for the next header */

  header_size = 256;
  next = store_get(sizeof(header_line));
  next->text = store_get(header_size);
  ptr = 0;
  had_zero = 0;
  prevlines_length = 0;
  }      /* Continue, starting to read the next header */

/* At this point, we have read all the headers into a data structure in main
store. The first header is still the dummy placeholder for the Received: header
we are going to generate a bit later on. If next != NULL, it contains the first
data line - which terminated the headers before reaching a blank line (not the
normal case). */

DEBUG(D_receive)
  {
  debug_printf(">>Headers received:\n");
  for (h = header_list->next; h; h = h->next)
    debug_printf("%s", h->text);
  debug_printf("\n");
  }

/* End of file on any SMTP connection is an error. If an incoming SMTP call
is dropped immediately after valid headers, the next thing we will see is EOF.
We must test for this specially, as further down the reading of the data is
skipped if already at EOF. */

if (smtp_input && (receive_feof)())
  {
  smtp_reply = handle_lost_connection(US" (after header)");
  smtp_yield = FALSE;
  goto TIDYUP;                       /* Skip to end of function */
  }

/* If this is a filter test run and no headers were read, output a warning
in case there is a mistake in the test message. */

if (filter_test != FTEST_NONE && header_list->next == NULL)
  printf("Warning: no message headers read\n");


/* Scan the headers to identify them. Some are merely marked for later
processing; some are dealt with here. */

for (h = header_list->next; h; h = h->next)
  {
  BOOL is_resent = strncmpic(h->text, US"resent-", 7) == 0;
  if (is_resent) contains_resent_headers = TRUE;

  switch (header_checkname(h, is_resent))
    {
    case htype_bcc:
    h->type = htype_bcc;        /* Both Bcc: and Resent-Bcc: */
    break;

    case htype_cc:
    h->type = htype_cc;         /* Both Cc: and Resent-Cc: */
    break;

    /* Record whether a Date: or Resent-Date: header exists, as appropriate. */

    case htype_date:
    if (!resents_exist || is_resent) date_header_exists = TRUE;
    break;

    /* Same comments as about Return-Path: below. */

    case htype_delivery_date:
    if (delivery_date_remove) h->type = htype_old;
    break;

    /* Same comments as about Return-Path: below. */

    case htype_envelope_to:
    if (envelope_to_remove) h->type = htype_old;
    break;

    /* Mark all "From:" headers so they get rewritten. Save the one that is to
    be used for Sender: checking. For Sendmail compatibility, if the "From:"
    header consists of just the login id of the user who called Exim, rewrite
    it with the gecos field first. Apply this rule to Resent-From: if there
    are resent- fields. */

    case htype_from:
    h->type = htype_from;
    if (!resents_exist || is_resent)
      {
      from_header = h;
      if (!smtp_input)
        {
        int len;
        uschar *s = Ustrchr(h->text, ':') + 1;
        while (isspace(*s)) s++;
        len = h->slen - (s - h->text) - 1;
        if (Ustrlen(originator_login) == len &&
	    strncmpic(s, originator_login, len) == 0)
          {
          uschar *name = is_resent? US"Resent-From" : US"From";
          header_add(htype_from, "%s: %s <%s@%s>\n", name, originator_name,
            originator_login, qualify_domain_sender);
          from_header = header_last;
          h->type = htype_old;
          DEBUG(D_receive|D_rewrite)
            debug_printf("rewrote \"%s:\" header using gecos\n", name);
         }
        }
      }
    break;

    /* Identify the Message-id: header for generating "in-reply-to" in the
    autoreply transport. For incoming logging, save any resent- value. In both
    cases, take just the first of any multiples. */

    case htype_id:
    if (msgid_header == NULL && (!resents_exist || is_resent))
      {
      msgid_header = h;
      h->type = htype_id;
      }
    break;

    /* Flag all Received: headers */

    case htype_received:
    h->type = htype_received;
    received_count++;
    break;

    /* "Reply-to:" is just noted (there is no resent-reply-to field) */

    case htype_reply_to:
    h->type = htype_reply_to;
    break;

    /* The Return-path: header is supposed to be added to messages when
    they leave the SMTP system. We shouldn't receive messages that already
    contain Return-path. However, since Exim generates Return-path: on
    local delivery, resent messages may well contain it. We therefore
    provide an option (which defaults on) to remove any Return-path: headers
    on input. Removal actually means flagging as "old", which prevents the
    header being transmitted with the message. */

    case htype_return_path:
    if (return_path_remove) h->type = htype_old;

    /* If we are testing a mail filter file, use the value of the
    Return-Path: header to set up the return_path variable, which is not
    otherwise set. However, remove any <> that surround the address
    because the variable doesn't have these. */

    if (filter_test != FTEST_NONE)
      {
      uschar *start = h->text + 12;
      uschar *end = start + Ustrlen(start);
      while (isspace(*start)) start++;
      while (end > start && isspace(end[-1])) end--;
      if (*start == '<' && end[-1] == '>')
        {
        start++;
        end--;
        }
      return_path = string_copyn(start, end - start);
      printf("Return-path taken from \"Return-path:\" header line\n");
      }
    break;

    /* If there is a "Sender:" header and the message is locally originated,
    and from an untrusted caller and suppress_local_fixups is not set, or if we
    are in submission mode for a remote message, mark it "old" so that it will
    not be transmitted with the message, unless active_local_sender_retain is
    set. (This can only be true if active_local_from_check is false.) If there
    are any resent- headers in the message, apply this rule to Resent-Sender:
    instead of Sender:. Messages with multiple resent- header sets cannot be
    tidily handled. (For this reason, at least one MUA - Pine - turns old
    resent- headers into X-resent- headers when resending, leaving just one
    set.) */

    case htype_sender:
    h->type = ((!active_local_sender_retain &&
                (
                (sender_local && !trusted_caller && !suppress_local_fixups)
                  || submission_mode
                )
               ) &&
               (!resents_exist||is_resent))?
      htype_old : htype_sender;
    break;

    /* Remember the Subject: header for logging. There is no Resent-Subject */

    case htype_subject:
    subject_header = h;
    break;

    /* "To:" gets flagged, and the existence of a recipient header is noted,
    whether it's resent- or not. */

    case htype_to:
    h->type = htype_to;
    /****
    to_or_cc_header_exists = TRUE;
    ****/
    break;
    }
  }

/* Extract recipients from the headers if that is required (the -t option).
Note that this is documented as being done *before* any address rewriting takes
place. There are two possibilities:

(1) According to sendmail documentation for Solaris, IRIX, and HP-UX, any
recipients already listed are to be REMOVED from the message. Smail 3 works
like this. We need to build a non-recipients tree for that list, because in
subsequent processing this data is held in a tree and that's what the
spool_write_header() function expects. Make sure that non-recipient addresses
are fully qualified and rewritten if necessary.

(2) According to other sendmail documentation, -t ADDS extracted recipients to
those in the command line arguments (and it is rumoured some other MTAs do
this). Therefore, there is an option to make Exim behave this way.

*** Notes on "Resent-" header lines ***

The presence of resent-headers in the message makes -t horribly ambiguous.
Experiments with sendmail showed that it uses recipients for all resent-
headers, totally ignoring the concept of "sets of resent- headers" as described
in RFC 2822 section 3.6.6. Sendmail also amalgamates them into a single set
with all the addresses in one instance of each header.

This seems to me not to be at all sensible. Before release 4.20, Exim 4 gave an
error for -t if there were resent- headers in the message. However, after a
discussion on the mailing list, I've learned that there are MUAs that use
resent- headers with -t, and also that the stuff about sets of resent- headers
and their ordering in RFC 2822 is generally ignored. An MUA that submits a
message with -t and resent- header lines makes sure that only *its* resent-
headers are present; previous ones are often renamed as X-resent- for example.

Consequently, Exim has been changed so that, if any resent- header lines are
present, the recipients are taken from all of the appropriate resent- lines,
and not from the ordinary To:, Cc:, etc. */

if (extract_recip)
  {
  int rcount = 0;
  error_block **bnext = &bad_addresses;

  if (extract_addresses_remove_arguments)
    {
    while (recipients_count-- > 0)
      {
      uschar *s = rewrite_address(recipients_list[recipients_count].address,
        TRUE, TRUE, global_rewrite_rules, rewrite_existflags);
      tree_add_nonrecipient(s);
      }
    recipients_list = NULL;
    recipients_count = recipients_list_max = 0;
    }

  /* Now scan the headers */

  for (h = header_list->next; h; h = h->next)
    {
    if ((h->type == htype_to || h->type == htype_cc || h->type == htype_bcc) &&
        (!contains_resent_headers || strncmpic(h->text, US"resent-", 7) == 0))
      {
      uschar *s = Ustrchr(h->text, ':') + 1;
      while (isspace(*s)) s++;

      parse_allow_group = TRUE;          /* Allow address group syntax */

      while (*s != 0)
        {
        uschar *ss = parse_find_address_end(s, FALSE);
        uschar *recipient, *errmess, *p, *pp;
        int start, end, domain;

        /* Check on maximum */

        if (recipients_max > 0 && ++rcount > recipients_max)
          {
          give_local_error(ERRMESS_TOOMANYRECIP, US"too many recipients",
            US"message rejected: ", error_rc, stdin, NULL);
          /* Does not return */
          }

        /* Make a copy of the address, and remove any internal newlines. These
        may be present as a result of continuations of the header line. The
        white space that follows the newline must not be removed - it is part
        of the header. */

        pp = recipient = store_get(ss - s + 1);
        for (p = s; p < ss; p++) if (*p != '\n') *pp++ = *p;
        *pp = 0;

#ifdef SUPPORT_I18N
	{
	BOOL b = allow_utf8_domains;
	allow_utf8_domains = TRUE;
#endif
        recipient = parse_extract_address(recipient, &errmess, &start, &end,
          &domain, FALSE);

#ifdef SUPPORT_I18N
	if (string_is_utf8(recipient))
	  message_smtputf8 = TRUE;
	else
	  allow_utf8_domains = b;
	}
#endif

        /* Keep a list of all the bad addresses so we can send a single
        error message at the end. However, an empty address is not an error;
        just ignore it. This can come from an empty group list like

          To: Recipients of list:;

        If there are no recipients at all, an error will occur later. */

        if (recipient == NULL && Ustrcmp(errmess, "empty address") != 0)
          {
          int len = Ustrlen(s);
          error_block *b = store_get(sizeof(error_block));
          while (len > 0 && isspace(s[len-1])) len--;
          b->next = NULL;
          b->text1 = string_printing(string_copyn(s, len));
          b->text2 = errmess;
          *bnext = b;
          bnext = &(b->next);
          }

        /* If the recipient is already in the nonrecipients tree, it must
        have appeared on the command line with the option extract_addresses_
        remove_arguments set. Do not add it to the recipients, and keep a note
        that this has happened, in order to give a better error if there are
        no recipients left. */

        else if (recipient != NULL)
          {
          if (tree_search(tree_nonrecipients, recipient) == NULL)
            receive_add_recipient(recipient, -1);
          else
            extracted_ignored = TRUE;
          }

        /* Move on past this address */

        s = ss + (*ss? 1:0);
        while (isspace(*s)) s++;
        }    /* Next address */

      parse_allow_group = FALSE;      /* Reset group syntax flags */
      parse_found_group = FALSE;

      /* If this was the bcc: header, mark it "old", which means it
      will be kept on the spool, but not transmitted as part of the
      message. */

      if (h->type == htype_bcc) h->type = htype_old;
      }   /* For appropriate header line */
    }     /* For each header line */

  }

/* Now build the unique message id. This has changed several times over the
lifetime of Exim. This description was rewritten for Exim 4.14 (February 2003).
Retaining all the history in the comment has become too unwieldy - read
previous release sources if you want it.

The message ID has 3 parts: tttttt-pppppp-ss. Each part is a number in base 62.
The first part is the current time, in seconds. The second part is the current
pid. Both are large enough to hold 32-bit numbers in base 62. The third part
can hold a number in the range 0-3843. It used to be a computed sequence
number, but is now the fractional component of the current time in units of
1/2000 of a second (i.e. a value in the range 0-1999). After a message has been
received, Exim ensures that the timer has ticked at the appropriate level
before proceeding, to avoid duplication if the pid happened to be re-used
within the same time period. It seems likely that most messages will take at
least half a millisecond to be received, so no delay will normally be
necessary. At least for some time...

There is a modification when localhost_number is set. Formerly this was allowed
to be as large as 255. Now it is restricted to the range 0-16, and the final
component of the message id becomes (localhost_number * 200) + fractional time
in units of 1/200 of a second (i.e. a value in the range 0-3399).

Some not-really-Unix operating systems use case-insensitive file names (Darwin,
Cygwin). For these, we have to use base 36 instead of base 62. Luckily, this
still allows the tttttt field to hold a large enough number to last for some
more decades, and the final two-digit field can hold numbers up to 1295, which
is enough for milliseconds (instead of 1/2000 of a second).

However, the pppppp field cannot hold a 32-bit pid, but it can hold a 31-bit
pid, so it is probably safe because pids have to be positive. The
localhost_number is restricted to 0-10 for these hosts, and when it is set, the
final field becomes (localhost_number * 100) + fractional time in centiseconds.

Note that string_base62() returns its data in a static storage block, so it
must be copied before calling string_base62() again. It always returns exactly
6 characters.

There doesn't seem to be anything in the RFC which requires a message id to
start with a letter, but Smail was changed to ensure this. The external form of
the message id (as supplied by string expansion) therefore starts with an
additional leading 'E'. The spool file names do not include this leading
letter and it is not used internally.

NOTE: If ever the format of message ids is changed, the regular expression for
checking that a string is in this format must be updated in a corresponding
way. It appears in the initializing code in exim.c. The macro MESSAGE_ID_LENGTH
must also be changed to reflect the correct string length. The queue-sort code
needs to know the layout. Then, of course, other programs that rely on the
message id format will need updating too. */

Ustrncpy(message_id, string_base62((long int)(message_id_tv.tv_sec)), 6);
message_id[6] = '-';
Ustrncpy(message_id + 7, string_base62((long int)getpid()), 6);

/* Deal with the case where the host number is set. The value of the number was
checked when it was read, to ensure it isn't too big. The timing granularity is
left in id_resolution so that an appropriate wait can be done after receiving
the message, if necessary (we hope it won't be). */

if (host_number_string != NULL)
  {
  id_resolution = (BASE_62 == 62)? 5000 : 10000;
  sprintf(CS(message_id + MESSAGE_ID_LENGTH - 3), "-%2s",
    string_base62((long int)(
      host_number * (1000000/id_resolution) +
        message_id_tv.tv_usec/id_resolution)) + 4);
  }

/* Host number not set: final field is just the fractional time at an
appropriate resolution. */

else
  {
  id_resolution = (BASE_62 == 62)? 500 : 1000;
  sprintf(CS(message_id + MESSAGE_ID_LENGTH - 3), "-%2s",
    string_base62((long int)(message_id_tv.tv_usec/id_resolution)) + 4);
  }

/* Add the current message id onto the current process info string if
it will fit. */

(void)string_format(process_info + process_info_len,
  PROCESS_INFO_SIZE - process_info_len, " id=%s", message_id);

/* If we are using multiple input directories, set up the one for this message
to be the least significant base-62 digit of the time of arrival. Otherwise
ensure that it is an empty string. */

message_subdir[0] = split_spool_directory ? message_id[5] : 0;

/* Now that we have the message-id, if there is no message-id: header, generate
one, but only for local (without suppress_local_fixups) or submission mode
messages. This can be user-configured if required, but we had better flatten
any illegal characters therein. */

if (msgid_header == NULL &&
      ((sender_host_address == NULL && !suppress_local_fixups)
        || submission_mode))
  {
  uschar *p;
  uschar *id_text = US"";
  uschar *id_domain = primary_hostname;

  /* Permit only letters, digits, dots, and hyphens in the domain */

  if (message_id_domain != NULL)
    {
    uschar *new_id_domain = expand_string(message_id_domain);
    if (new_id_domain == NULL)
      {
      if (!expand_string_forcedfail)
        log_write(0, LOG_MAIN|LOG_PANIC,
          "expansion of \"%s\" (message_id_header_domain) "
          "failed: %s", message_id_domain, expand_string_message);
      }
    else if (*new_id_domain != 0)
      {
      id_domain = new_id_domain;
      for (p = id_domain; *p != 0; p++)
        if (!isalnum(*p) && *p != '.') *p = '-';  /* No need to test '-' ! */
      }
    }

  /* Permit all characters except controls and RFC 2822 specials in the
  additional text part. */

  if (message_id_text != NULL)
    {
    uschar *new_id_text = expand_string(message_id_text);
    if (new_id_text == NULL)
      {
      if (!expand_string_forcedfail)
        log_write(0, LOG_MAIN|LOG_PANIC,
          "expansion of \"%s\" (message_id_header_text) "
          "failed: %s", message_id_text, expand_string_message);
      }
    else if (*new_id_text != 0)
      {
      id_text = new_id_text;
      for (p = id_text; *p != 0; p++)
        if (mac_iscntrl_or_special(*p)) *p = '-';
      }
    }

  /* Add the header line
   * Resent-* headers are prepended, per RFC 5322 3.6.6.  Non-Resent-* are
   * appended, to preserve classical expectations of header ordering. */

  header_add_at_position(!resents_exist, NULL, FALSE, htype_id,
    "%sMessage-Id: <%s%s%s@%s>\n", resent_prefix, message_id_external,
    (*id_text == 0)? "" : ".", id_text, id_domain);
  }

/* If we are to log recipients, keep a copy of the raw ones before any possible
rewriting. Must copy the count, because later ACLs and the local_scan()
function may mess with the real recipients. */

if (LOGGING(received_recipients))
  {
  raw_recipients = store_get(recipients_count * sizeof(uschar *));
  for (i = 0; i < recipients_count; i++)
    raw_recipients[i] = string_copy(recipients_list[i].address);
  raw_recipients_count = recipients_count;
  }

/* Ensure the recipients list is fully qualified and rewritten. Unqualified
recipients will get here only if the conditions were right (allow_unqualified_
recipient is TRUE). */

for (i = 0; i < recipients_count; i++)
  recipients_list[i].address =
    rewrite_address(recipients_list[i].address, TRUE, TRUE,
      global_rewrite_rules, rewrite_existflags);

/* If there is no From: header, generate one for local (without
suppress_local_fixups) or submission_mode messages. If there is no sender
address, but the sender is local or this is a local delivery error, use the
originator login. This shouldn't happen for genuine bounces, but might happen
for autoreplies. The addition of From: must be done *before* checking for the
possible addition of a Sender: header, because untrusted_set_sender allows an
untrusted user to set anything in the envelope (which might then get info
From:) but we still want to ensure a valid Sender: if it is required. */

if (from_header == NULL &&
    ((sender_host_address == NULL && !suppress_local_fixups)
      || submission_mode))
  {
  uschar *oname = US"";

  /* Use the originator_name if this is a locally submitted message and the
  caller is not trusted. For trusted callers, use it only if -F was used to
  force its value or if we have a non-SMTP message for which -f was not used
  to set the sender. */

  if (sender_host_address == NULL)
    {
    if (!trusted_caller || sender_name_forced ||
         (!smtp_input && !sender_address_forced))
      oname = originator_name;
    }

  /* For non-locally submitted messages, the only time we use the originator
  name is when it was forced by the /name= option on control=submission. */

  else
    {
    if (submission_name != NULL) oname = submission_name;
    }

  /* Envelope sender is empty */

  if (sender_address[0] == 0)
    {
    uschar *fromstart, *fromend;

    fromstart = string_sprintf("%sFrom: %s%s", resent_prefix,
      oname, (oname[0] == 0)? "" : " <");
    fromend = (oname[0] == 0)? US"" : US">";

    if (sender_local || local_error_message)
      {
      header_add(htype_from, "%s%s@%s%s\n", fromstart,
        local_part_quote(originator_login), qualify_domain_sender,
        fromend);
      }
    else if (submission_mode && authenticated_id != NULL)
      {
      if (submission_domain == NULL)
        {
        header_add(htype_from, "%s%s@%s%s\n", fromstart,
          local_part_quote(authenticated_id), qualify_domain_sender,
          fromend);
        }
      else if (submission_domain[0] == 0)  /* empty => whole address set */
        {
        header_add(htype_from, "%s%s%s\n", fromstart, authenticated_id,
          fromend);
        }
      else
        {
        header_add(htype_from, "%s%s@%s%s\n", fromstart,
          local_part_quote(authenticated_id), submission_domain,
          fromend);
        }
      from_header = header_last;    /* To get it checked for Sender: */
      }
    }

  /* There is a non-null envelope sender. Build the header using the original
  sender address, before any rewriting that might have been done while
  verifying it. */

  else
    {
    header_add(htype_from, "%sFrom: %s%s%s%s\n", resent_prefix,
      oname,
      (oname[0] == 0)? "" : " <",
      (sender_address_unrewritten == NULL)?
        sender_address : sender_address_unrewritten,
      (oname[0] == 0)? "" : ">");

    from_header = header_last;    /* To get it checked for Sender: */
    }
  }


/* If the sender is local (without suppress_local_fixups), or if we are in
submission mode and there is an authenticated_id, check that an existing From:
is correct, and if not, generate a Sender: header, unless disabled. Any
previously-existing Sender: header was removed above. Note that sender_local,
as well as being TRUE if the caller of exim is not trusted, is also true if a
trusted caller did not supply a -f argument for non-smtp input. To allow
trusted callers to forge From: without supplying -f, we have to test explicitly
here. If the From: header contains more than one address, then the call to
parse_extract_address fails, and a Sender: header is inserted, as required. */

if (from_header != NULL &&
     (active_local_from_check &&
       ((sender_local && !trusted_caller && !suppress_local_fixups) ||
        (submission_mode && authenticated_id != NULL))
     ))
  {
  BOOL make_sender = TRUE;
  int start, end, domain;
  uschar *errmess;
  uschar *from_address =
    parse_extract_address(Ustrchr(from_header->text, ':') + 1, &errmess,
      &start, &end, &domain, FALSE);
  uschar *generated_sender_address;

  if (submission_mode)
    {
    if (submission_domain == NULL)
      {
      generated_sender_address = string_sprintf("%s@%s",
        local_part_quote(authenticated_id), qualify_domain_sender);
      }
    else if (submission_domain[0] == 0)  /* empty => full address */
      {
      generated_sender_address = string_sprintf("%s",
        authenticated_id);
      }
    else
      {
      generated_sender_address = string_sprintf("%s@%s",
        local_part_quote(authenticated_id), submission_domain);
      }
    }
  else
    generated_sender_address = string_sprintf("%s@%s",
      local_part_quote(originator_login), qualify_domain_sender);

  /* Remove permitted prefixes and suffixes from the local part of the From:
  address before doing the comparison with the generated sender. */

  if (from_address != NULL)
    {
    int slen;
    uschar *at = (domain == 0)? NULL : from_address + domain - 1;

    if (at != NULL) *at = 0;
    from_address += route_check_prefix(from_address, local_from_prefix);
    slen = route_check_suffix(from_address, local_from_suffix);
    if (slen > 0)
      {
      memmove(from_address+slen, from_address, Ustrlen(from_address)-slen);
      from_address += slen;
      }
    if (at != NULL) *at = '@';

    if (strcmpic(generated_sender_address, from_address) == 0 ||
      (domain == 0 && strcmpic(from_address, originator_login) == 0))
        make_sender = FALSE;
    }

  /* We have to cause the Sender header to be rewritten if there are
  appropriate rewriting rules. */

  if (make_sender)
    {
    if (submission_mode && submission_name == NULL)
      header_add(htype_sender, "%sSender: %s\n", resent_prefix,
        generated_sender_address);
    else
      header_add(htype_sender, "%sSender: %s <%s>\n",
        resent_prefix,
        submission_mode? submission_name : originator_name,
        generated_sender_address);
    }

  /* Ensure that a non-null envelope sender address corresponds to the
  submission mode sender address. */

  if (submission_mode && sender_address[0] != 0)
    {
    if (sender_address_unrewritten == NULL)
      sender_address_unrewritten = sender_address;
    sender_address = generated_sender_address;
    if (Ustrcmp(sender_address_unrewritten, generated_sender_address) != 0)
      log_write(L_address_rewrite, LOG_MAIN,
        "\"%s\" from env-from rewritten as \"%s\" by submission mode",
        sender_address_unrewritten, generated_sender_address);
    }
  }

/* If there are any rewriting rules, apply them to the sender address, unless
it has already been rewritten as part of verification for SMTP input. */

if (global_rewrite_rules != NULL && sender_address_unrewritten == NULL &&
    sender_address[0] != 0)
  {
  sender_address = rewrite_address(sender_address, FALSE, TRUE,
    global_rewrite_rules, rewrite_existflags);
  DEBUG(D_receive|D_rewrite)
    debug_printf("rewritten sender = %s\n", sender_address);
  }


/* The headers must be run through rewrite_header(), because it ensures that
addresses are fully qualified, as well as applying any rewriting rules that may
exist.

Qualification of header addresses in a message from a remote host happens only
if the host is in sender_unqualified_hosts or recipient_unqualified hosts, as
appropriate. For local messages, qualification always happens, unless -bnq is
used to explicitly suppress it. No rewriting is done for an unqualified address
that is left untouched.

We start at the second header, skipping our own Received:. This rewriting is
documented as happening *after* recipient addresses are taken from the headers
by the -t command line option. An added Sender: gets rewritten here. */

for (h = header_list->next; h; h = h->next)
  {
  header_line *newh = rewrite_header(h, NULL, NULL, global_rewrite_rules,
    rewrite_existflags, TRUE);
  if (newh) h = newh;
  }


/* An RFC 822 (sic) message is not legal unless it has at least one of "to",
"cc", or "bcc". Note that although the minimal examples in RFC 822 show just
"to" or "bcc", the full syntax spec allows "cc" as well. If any resent- header
exists, this applies to the set of resent- headers rather than the normal set.

The requirement for a recipient header has been removed in RFC 2822. At this
point in the code, earlier versions of Exim added a To: header for locally
submitted messages, and an empty Bcc: header for others. In the light of the
changes in RFC 2822, this was dropped in November 2003. */


/* If there is no date header, generate one if the message originates locally
(i.e. not over TCP/IP) and suppress_local_fixups is not set, or if the
submission mode flag is set. Messages without Date: are not valid, but it seems
to be more confusing if Exim adds one to all remotely-originated messages.
As per Message-Id, we prepend if resending, else append.
*/

if (!date_header_exists &&
      ((sender_host_address == NULL && !suppress_local_fixups)
        || submission_mode))
  header_add_at_position(!resents_exist, NULL, FALSE, htype_other,
    "%sDate: %s\n", resent_prefix, tod_stamp(tod_full));

search_tidyup();    /* Free any cached resources */

/* Show the complete set of headers if debugging. Note that the first one (the
new Received:) has not yet been set. */

DEBUG(D_receive)
  {
  debug_printf(">>Headers after rewriting and local additions:\n");
  for (h = header_list->next; h != NULL; h = h->next)
    debug_printf("%c %s", h->type, h->text);
  debug_printf("\n");
  }

/* The headers are now complete in store. If we are running in filter
testing mode, that is all this function does. Return TRUE if the message
ended with a dot. */

if (filter_test != FTEST_NONE)
  {
  process_info[process_info_len] = 0;
  return message_ended == END_DOT;
  }

/*XXX CHUNKING: need to cancel cutthrough under BDAT, for now.  In future,
think more if it could be handled.  Cannot do onward CHUNKING unless
inbound is, but inbound chunking ought to be ok with outbound plain.
Could we do onward CHUNKING given inbound CHUNKING?
*/
if (chunking_state > CHUNKING_OFFERED)
  cancel_cutthrough_connection(FALSE, US"chunking active");

/* Cutthrough delivery:
We have to create the Received header now rather than at the end of reception,
so the timestamp behaviour is a change to the normal case.
Having created it, send the headers to the destination. */

if (cutthrough.fd >= 0 && cutthrough.delivery)
  {
  if (received_count > received_headers_max)
    {
    cancel_cutthrough_connection(TRUE, US"too many headers");
    if (smtp_input) receive_swallow_smtp();  /* Swallow incoming SMTP */
    log_write(0, LOG_MAIN|LOG_REJECT, "rejected from <%s>%s%s%s%s: "
      "Too many \"Received\" headers",
      sender_address,
      sender_fullhost ? "H=" : "", sender_fullhost ? sender_fullhost : US"",
      sender_ident ? "U=" : "", sender_ident ? sender_ident : US"");
    message_id[0] = 0;                       /* Indicate no message accepted */
    smtp_reply = US"550 Too many \"Received\" headers - suspected mail loop";
    goto TIDYUP;                             /* Skip to end of function */
    }
  received_header_gen();
  add_acl_headers(ACL_WHERE_RCPT, US"MAIL or RCPT");
  (void) cutthrough_headers_send();
  }


/* Open a new spool file for the data portion of the message. We need
to access it both via a file descriptor and a stream. Try to make the
directory if it isn't there. */

spool_name = spool_fname(US"input", message_subdir, message_id, US"-D");
DEBUG(D_receive) debug_printf("Data file name: %s\n", spool_name);

if ((data_fd = Uopen(spool_name, O_RDWR|O_CREAT|O_EXCL, SPOOL_MODE)) < 0)
  {
  if (errno == ENOENT)
    {
    (void) directory_make(spool_directory,
		        spool_sname(US"input", message_subdir),
			INPUT_DIRECTORY_MODE, TRUE);
    data_fd = Uopen(spool_name, O_RDWR|O_CREAT|O_EXCL, SPOOL_MODE);
    }
  if (data_fd < 0)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "Failed to create spool file %s: %s",
      spool_name, strerror(errno));
  }

/* Make sure the file's group is the Exim gid, and double-check the mode
because the group setting doesn't always get set automatically. */

if (fchown(data_fd, exim_uid, exim_gid))
  log_write(0, LOG_MAIN|LOG_PANIC_DIE,
    "Failed setting ownership on spool file %s: %s",
    spool_name, strerror(errno));
(void)fchmod(data_fd, SPOOL_MODE);

/* We now have data file open. Build a stream for it and lock it. We lock only
the first line of the file (containing the message ID) because otherwise there
are problems when Exim is run under Cygwin (I'm told). See comments in
spool_in.c, where the same locking is done. */

data_file = fdopen(data_fd, "w+");
lock_data.l_type = F_WRLCK;
lock_data.l_whence = SEEK_SET;
lock_data.l_start = 0;
lock_data.l_len = SPOOL_DATA_START_OFFSET;

if (fcntl(data_fd, F_SETLK, &lock_data) < 0)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE, "Cannot lock %s (%d): %s", spool_name,
    errno, strerror(errno));

/* We have an open, locked data file. Write the message id to it to make it
self-identifying. Then read the remainder of the input of this message and
write it to the data file. If the variable next != NULL, it contains the first
data line (which was read as a header but then turned out not to have the right
format); write it (remembering that it might contain binary zeros). The result
of fwrite() isn't inspected; instead we call ferror() below. */

fprintf(data_file, "%s-D\n", message_id);
if (next != NULL)
  {
  uschar *s = next->text;
  int len = next->slen;
  len = fwrite(s, 1, len, data_file);  len = len; /* compiler quietening */
  body_linecount++;                 /* Assumes only 1 line */
  }

/* Note that we might already be at end of file, or the logical end of file
(indicated by '.'), or might have encountered an error while writing the
message id or "next" line. */

if (!ferror(data_file) && !(receive_feof)() && message_ended != END_DOT)
  {
  if (smtp_input)
    {
    message_ended = chunking_state <= CHUNKING_OFFERED
      ? read_message_data_smtp(data_file)
      : spool_wireformat
      ? read_message_bdat_smtp_wire(data_file)
      : read_message_bdat_smtp(data_file);
    receive_linecount++;                /* The terminating "." line */
    }
  else message_ended = read_message_data(data_file);

  receive_linecount += body_linecount;  /* For BSMTP errors mainly */
  message_linecount += body_linecount;

  switch (message_ended)
    {
    /* Handle premature termination of SMTP */

    case END_EOF:
      if (smtp_input)
	{
	Uunlink(spool_name);                 /* Lose data file when closed */
	cancel_cutthrough_connection(TRUE, US"sender closed connection");
	message_id[0] = 0;                   /* Indicate no message accepted */
	smtp_reply = handle_lost_connection(US"");
	smtp_yield = FALSE;
	goto TIDYUP;                         /* Skip to end of function */
	}
      break;

    /* Handle message that is too big. Don't use host_or_ident() in the log
    message; we want to see the ident value even for non-remote messages. */

    case END_SIZE:
      Uunlink(spool_name);                /* Lose the data file when closed */
      cancel_cutthrough_connection(TRUE, US"mail too big");
      if (smtp_input) receive_swallow_smtp();  /* Swallow incoming SMTP */

      log_write(L_size_reject, LOG_MAIN|LOG_REJECT, "rejected from <%s>%s%s%s%s: "
	"message too big: read=%d max=%d",
	sender_address,
	(sender_fullhost == NULL)? "" : " H=",
	(sender_fullhost == NULL)? US"" : sender_fullhost,
	(sender_ident == NULL)? "" : " U=",
	(sender_ident == NULL)? US"" : sender_ident,
	message_size,
	thismessage_size_limit);

      if (smtp_input)
	{
	smtp_reply = US"552 Message size exceeds maximum permitted";
	message_id[0] = 0;               /* Indicate no message accepted */
	goto TIDYUP;                     /* Skip to end of function */
	}
      else
	{
	fseek(data_file, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);
	give_local_error(ERRMESS_TOOBIG,
	  string_sprintf("message too big (max=%d)", thismessage_size_limit),
	  US"message rejected: ", error_rc, data_file, header_list);
	/* Does not return */
	}
      break;

    /* Handle bad BDAT protocol sequence */

    case END_PROTOCOL:
      Uunlink(spool_name);		/* Lose the data file when closed */
      cancel_cutthrough_connection(TRUE, US"sender protocol error");
      smtp_reply = US"";		/* Response already sent */
      message_id[0] = 0;		/* Indicate no message accepted */
      goto TIDYUP;			/* Skip to end of function */
    }
  }

/* Restore the standard SIGALRM handler for any subsequent processing. (For
example, there may be some expansion in an ACL that uses a timer.) */

os_non_restarting_signal(SIGALRM, sigalrm_handler);

/* The message body has now been read into the data file. Call fflush() to
empty the buffers in C, and then call fsync() to get the data written out onto
the disk, as fflush() doesn't do this (or at least, it isn't documented as
having to do this). If there was an I/O error on either input or output,
attempt to send an error message, and unlink the spool file. For non-SMTP input
we can then give up. Note that for SMTP input we must swallow the remainder of
the input in cases of output errors, since the far end doesn't expect to see
anything until the terminating dot line is sent. */

if (fflush(data_file) == EOF || ferror(data_file) ||
    EXIMfsync(fileno(data_file)) < 0 || (receive_ferror)())
  {
  uschar *msg_errno = US strerror(errno);
  BOOL input_error = (receive_ferror)() != 0;
  uschar *msg = string_sprintf("%s error (%s) while receiving message from %s",
    input_error? "Input read" : "Spool write",
    msg_errno,
    (sender_fullhost != NULL)? sender_fullhost : sender_ident);

  log_write(0, LOG_MAIN, "Message abandoned: %s", msg);
  Uunlink(spool_name);                /* Lose the data file */
  cancel_cutthrough_connection(TRUE, US"error writing spoolfile");

  if (smtp_input)
    {
    if (input_error)
      smtp_reply = US"451 Error while reading input data";
    else
      {
      smtp_reply = US"451 Error while writing spool file";
      receive_swallow_smtp();
      }
    message_id[0] = 0;               /* Indicate no message accepted */
    goto TIDYUP;                     /* Skip to end of function */
    }

  else
    {
    fseek(data_file, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);
    give_local_error(ERRMESS_IOERR, msg, US"", error_rc, data_file,
      header_list);
    /* Does not return */
    }
  }


/* No I/O errors were encountered while writing the data file. */

DEBUG(D_receive) debug_printf("Data file written for message %s\n", message_id);


/* If there were any bad addresses extracted by -t, or there were no recipients
left after -t, send a message to the sender of this message, or write it to
stderr if the error handling option is set that way. Note that there may
legitimately be no recipients for an SMTP message if they have all been removed
by "discard".

We need to rewind the data file in order to read it. In the case of no
recipients or stderr error writing, throw the data file away afterwards, and
exit. (This can't be SMTP, which always ensures there's at least one
syntactically good recipient address.) */

if (extract_recip && (bad_addresses != NULL || recipients_count == 0))
  {
  DEBUG(D_receive)
    {
    if (recipients_count == 0) debug_printf("*** No recipients\n");
    if (bad_addresses != NULL)
      {
      error_block *eblock = bad_addresses;
      debug_printf("*** Bad address(es)\n");
      while (eblock != NULL)
        {
        debug_printf("  %s: %s\n", eblock->text1, eblock->text2);
        eblock = eblock->next;
        }
      }
    }

  fseek(data_file, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);

  /* If configured to send errors to the sender, but this fails, force
  a failure error code. We use a special one for no recipients so that it
  can be detected by the autoreply transport. Otherwise error_rc is set to
  errors_sender_rc, which is EXIT_FAILURE unless -oee was given, in which case
  it is EXIT_SUCCESS. */

  if (error_handling == ERRORS_SENDER)
    {
    if (!moan_to_sender(
          (bad_addresses == NULL)?
            (extracted_ignored? ERRMESS_IGADDRESS : ERRMESS_NOADDRESS) :
          (recipients_list == NULL)? ERRMESS_BADNOADDRESS : ERRMESS_BADADDRESS,
          bad_addresses, header_list, data_file, FALSE))
      error_rc = (bad_addresses == NULL)? EXIT_NORECIPIENTS : EXIT_FAILURE;
    }
  else
    {
    if (bad_addresses == NULL)
      {
      if (extracted_ignored)
        fprintf(stderr, "exim: all -t recipients overridden by command line\n");
      else
        fprintf(stderr, "exim: no recipients in message\n");
      }
    else
      {
      fprintf(stderr, "exim: invalid address%s",
        (bad_addresses->next == NULL)? ":" : "es:\n");
      while (bad_addresses != NULL)
        {
        fprintf(stderr, "  %s: %s\n", bad_addresses->text1,
          bad_addresses->text2);
        bad_addresses = bad_addresses->next;
        }
      }
    }

  if (recipients_count == 0 || error_handling == ERRORS_STDERR)
    {
    Uunlink(spool_name);
    (void)fclose(data_file);
    exim_exit(error_rc, US"receiving");
    }
  }

/* Data file successfully written. Generate text for the Received: header by
expanding the configured string, and adding a timestamp. By leaving this
operation till now, we ensure that the timestamp is the time that message
reception was completed. However, this is deliberately done before calling the
data ACL and local_scan().

This Received: header may therefore be inspected by the data ACL and by code in
the local_scan() function. When they have run, we update the timestamp to be
the final time of reception.

If there is just one recipient, set up its value in the $received_for variable
for use when we generate the Received: header.

Note: the checking for too many Received: headers is handled by the delivery
code. */
/*XXX eventually add excess Received: check for cutthrough case back when classifying them */

if (received_header->text == NULL)	/* Non-cutthrough case */
  {
  received_header_gen();

  /* Set the value of message_body_size for the DATA ACL and for local_scan() */

  message_body_size = (fstat(data_fd, &statbuf) == 0)?
    statbuf.st_size - SPOOL_DATA_START_OFFSET : -1;

  /* If an ACL from any RCPT commands set up any warning headers to add, do so
  now, before running the DATA ACL. */

  add_acl_headers(ACL_WHERE_RCPT, US"MAIL or RCPT");
  }
else
  message_body_size = (fstat(data_fd, &statbuf) == 0)?
    statbuf.st_size - SPOOL_DATA_START_OFFSET : -1;

/* If an ACL is specified for checking things at this stage of reception of a
message, run it, unless all the recipients were removed by "discard" in earlier
ACLs. That is the only case in which recipients_count can be zero at this
stage. Set deliver_datafile to point to the data file so that $message_body and
$message_body_end can be extracted if needed. Allow $recipients in expansions.
*/

deliver_datafile = data_fd;
user_msg = NULL;

enable_dollar_recipients = TRUE;

if (recipients_count == 0)
  blackholed_by = recipients_discarded ? US"MAIL ACL" : US"RCPT ACL";

else
  {
  /* Handle interactive SMTP messages */

  if (smtp_input && !smtp_batched_input)
    {

#ifndef DISABLE_DKIM
    if (!dkim_disable_verify)
      {
      /* Finish verification */
      dkim_exim_verify_finish();

      /* Check if we must run the DKIM ACL */
      if (acl_smtp_dkim && dkim_verify_signers && *dkim_verify_signers)
        {
        uschar * dkim_verify_signers_expanded =
          expand_string(dkim_verify_signers);
	gstring * results = NULL;
	int signer_sep = 0;
	const uschar * ptr;
	uschar * item;
	gstring * seen_items = NULL;
	int old_pool = store_pool;

	store_pool = POOL_PERM;   /* Allow created variables to live to data ACL */

        if (!(ptr = dkim_verify_signers_expanded))
          log_write(0, LOG_MAIN|LOG_PANIC,
            "expansion of dkim_verify_signers option failed: %s",
            expand_string_message);

	/* Default to OK when no items are present */
	rc = OK;
	while ((item = string_nextinlist(&ptr, &signer_sep, NULL, 0)))
	  {
	  /* Prevent running ACL for an empty item */
	  if (!item || !*item) continue;

	  /* Only run ACL once for each domain or identity,
	  no matter how often it appears in the expanded list. */
	  if (seen_items)
	    {
	    uschar * seen_item;
	    const uschar * seen_items_list = string_from_gstring(seen_items);
	    int seen_sep = ':';
	    BOOL seen_this_item = FALSE;

	    while ((seen_item = string_nextinlist(&seen_items_list, &seen_sep,
						  NULL, 0)))
	      if (Ustrcmp(seen_item,item) == 0)
		{
		seen_this_item = TRUE;
		break;
		}

	    if (seen_this_item)
	      {
	      DEBUG(D_receive)
		debug_printf("acl_smtp_dkim: skipping signer %s, "
		  "already seen\n", item);
	      continue;
	      }

	    seen_items = string_catn(seen_items, ":", 1);
	    }
	  seen_items = string_cat(seen_items, item);

	  rc = dkim_exim_acl_run(item, &results, &user_msg, &log_msg);
	  if (rc != OK)
	    {
	    DEBUG(D_receive)
	      debug_printf("acl_smtp_dkim: acl_check returned %d on %s, "
		"skipping remaining items\n", rc, item);
	    cancel_cutthrough_connection(TRUE, US"dkim acl not ok");
	    break;
	    }
	  }
	dkim_verify_status = string_from_gstring(results);
	store_pool = old_pool;
	add_acl_headers(ACL_WHERE_DKIM, US"DKIM");
	if (rc == DISCARD)
	  {
	  recipients_count = 0;
	  blackholed_by = US"DKIM ACL";
	  if (log_msg)
	    blackhole_log_msg = string_sprintf(": %s", log_msg);
	  }
	else if (rc != OK)
	  {
	  Uunlink(spool_name);
	  if (smtp_handle_acl_fail(ACL_WHERE_DKIM, rc, user_msg, log_msg) != 0)
	    smtp_yield = FALSE;    /* No more messages after dropped connection */
	  smtp_reply = US"";       /* Indicate reply already sent */
	  message_id[0] = 0;       /* Indicate no message accepted */
	  goto TIDYUP;             /* Skip to end of function */
	  }
        }
      else
	dkim_exim_verify_log_all();
      }
#endif /* DISABLE_DKIM */

#ifdef WITH_CONTENT_SCAN
    if (recipients_count > 0 &&
        acl_smtp_mime != NULL &&
        !run_mime_acl(acl_smtp_mime, &smtp_yield, &smtp_reply, &blackholed_by))
      goto TIDYUP;
#endif /* WITH_CONTENT_SCAN */

#ifdef EXPERIMENTAL_DMARC
    dmarc_up = dmarc_store_data(from_header);
#endif /* EXPERIMENTAL_DMARC */

#ifndef DISABLE_PRDR
    if (prdr_requested && recipients_count > 1 && acl_smtp_data_prdr)
      {
      unsigned int c;
      int all_pass = OK;
      int all_fail = FAIL;

      smtp_printf("353 PRDR content analysis beginning\r\n", TRUE);
      /* Loop through recipients, responses must be in same order received */
      for (c = 0; recipients_count > c; c++)
        {
	uschar * addr= recipients_list[c].address;
	uschar * msg= US"PRDR R=<%s> %s";
	uschar * code;
        DEBUG(D_receive)
          debug_printf("PRDR processing recipient %s (%d of %d)\n",
                       addr, c+1, recipients_count);
        rc = acl_check(ACL_WHERE_PRDR, addr,
                       acl_smtp_data_prdr, &user_msg, &log_msg);

        /* If any recipient rejected content, indicate it in final message */
        all_pass |= rc;
        /* If all recipients rejected, indicate in final message */
        all_fail &= rc;

        switch (rc)
          {
          case OK: case DISCARD: code = US"250"; break;
          case DEFER:            code = US"450"; break;
          default:               code = US"550"; break;
          }
	if (user_msg != NULL)
	  smtp_user_msg(code, user_msg);
	else
	  {
	  switch (rc)
            {
            case OK: case DISCARD:
              msg = string_sprintf(CS msg, addr, "acceptance");        break;
            case DEFER:
              msg = string_sprintf(CS msg, addr, "temporary refusal"); break;
            default:
              msg = string_sprintf(CS msg, addr, "refusal");           break;
            }
          smtp_user_msg(code, msg);
	  }
	if (log_msg)       log_write(0, LOG_MAIN, "PRDR %s %s", addr, log_msg);
	else if (user_msg) log_write(0, LOG_MAIN, "PRDR %s %s", addr, user_msg);
	else               log_write(0, LOG_MAIN, "%s", CS msg);

	if (rc != OK) { receive_remove_recipient(addr); c--; }
        }
      /* Set up final message, used if data acl gives OK */
      smtp_reply = string_sprintf("%s id=%s message %s",
		       all_fail == FAIL ? US"550" : US"250",
		       message_id,
                       all_fail == FAIL
		         ? US"rejected for all recipients"
			 : all_pass == OK
			   ? US"accepted"
			   : US"accepted for some recipients");
      if (recipients_count == 0)
        {
        message_id[0] = 0;       /* Indicate no message accepted */
	goto TIDYUP;
	}
      }
    else
      prdr_requested = FALSE;
#endif /* !DISABLE_PRDR */

    /* Check the recipients count again, as the MIME ACL might have changed
    them. */

    if (acl_smtp_data != NULL && recipients_count > 0)
      {
      rc = acl_check(ACL_WHERE_DATA, NULL, acl_smtp_data, &user_msg, &log_msg);
      add_acl_headers(ACL_WHERE_DATA, US"DATA");
      if (rc == DISCARD)
        {
        recipients_count = 0;
        blackholed_by = US"DATA ACL";
        if (log_msg)
          blackhole_log_msg = string_sprintf(": %s", log_msg);
	cancel_cutthrough_connection(TRUE, US"data acl discard");
        }
      else if (rc != OK)
        {
        Uunlink(spool_name);
	cancel_cutthrough_connection(TRUE, US"data acl not ok");
#ifdef WITH_CONTENT_SCAN
        unspool_mbox();
#endif
#ifdef EXPERIMENTAL_DCC
	dcc_ok = 0;
#endif
        if (smtp_handle_acl_fail(ACL_WHERE_DATA, rc, user_msg, log_msg) != 0)
          smtp_yield = FALSE;    /* No more messages after dropped connection */
        smtp_reply = US"";       /* Indicate reply already sent */
        message_id[0] = 0;       /* Indicate no message accepted */
        goto TIDYUP;             /* Skip to end of function */
        }
      }
    }

  /* Handle non-SMTP and batch SMTP (i.e. non-interactive) messages. Note that
  we cannot take different actions for permanent and temporary rejections. */

  else
    {

#ifdef WITH_CONTENT_SCAN
    if (acl_not_smtp_mime != NULL &&
        !run_mime_acl(acl_not_smtp_mime, &smtp_yield, &smtp_reply,
          &blackholed_by))
      goto TIDYUP;
#endif /* WITH_CONTENT_SCAN */

    if (acl_not_smtp != NULL)
      {
      uschar *user_msg, *log_msg;
      rc = acl_check(ACL_WHERE_NOTSMTP, NULL, acl_not_smtp, &user_msg, &log_msg);
      if (rc == DISCARD)
        {
        recipients_count = 0;
        blackholed_by = US"non-SMTP ACL";
        if (log_msg != NULL)
          blackhole_log_msg = string_sprintf(": %s", log_msg);
        }
      else if (rc != OK)
        {
        Uunlink(spool_name);
#ifdef WITH_CONTENT_SCAN
        unspool_mbox();
#endif
#ifdef EXPERIMENTAL_DCC
	dcc_ok = 0;
#endif
        /* The ACL can specify where rejections are to be logged, possibly
        nowhere. The default is main and reject logs. */

        if (log_reject_target != 0)
          log_write(0, log_reject_target, "F=<%s> rejected by non-SMTP ACL: %s",
            sender_address, log_msg);

        if (user_msg == NULL) user_msg = US"local configuration problem";
        if (smtp_batched_input)
          {
          moan_smtp_batch(NULL, "%d %s", 550, user_msg);
          /* Does not return */
          }
        else
          {
          fseek(data_file, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);
          give_local_error(ERRMESS_LOCAL_ACL, user_msg,
            US"message rejected by non-SMTP ACL: ", error_rc, data_file,
              header_list);
          /* Does not return */
          }
        }
      add_acl_headers(ACL_WHERE_NOTSMTP, US"non-SMTP");
      }
    }

  /* The applicable ACLs have been run */

  if (deliver_freeze) frozen_by = US"ACL";     /* for later logging */
  if (queue_only_policy) queued_by = US"ACL";
  }

#ifdef WITH_CONTENT_SCAN
unspool_mbox();
#endif

#ifdef EXPERIMENTAL_DCC
dcc_ok = 0;
#endif


/* The final check on the message is to run the scan_local() function. The
version supplied with Exim always accepts, but this is a hook for sysadmins to
supply their own checking code. The local_scan() function is run even when all
the recipients have been discarded. */
/*XXS could we avoid this for the standard case, given that few people will use it? */

lseek(data_fd, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);

/* Arrange to catch crashes in local_scan(), so that the -D file gets
deleted, and the incident gets logged. */

os_non_restarting_signal(SIGSEGV, local_scan_crash_handler);
os_non_restarting_signal(SIGFPE, local_scan_crash_handler);
os_non_restarting_signal(SIGILL, local_scan_crash_handler);
os_non_restarting_signal(SIGBUS, local_scan_crash_handler);

DEBUG(D_receive) debug_printf("calling local_scan(); timeout=%d\n",
  local_scan_timeout);
local_scan_data = NULL;

os_non_restarting_signal(SIGALRM, local_scan_timeout_handler);
if (local_scan_timeout > 0) alarm(local_scan_timeout);
rc = local_scan(data_fd, &local_scan_data);
alarm(0);
os_non_restarting_signal(SIGALRM, sigalrm_handler);

enable_dollar_recipients = FALSE;

store_pool = POOL_MAIN;   /* In case changed */
DEBUG(D_receive) debug_printf("local_scan() returned %d %s\n", rc,
  local_scan_data);

os_non_restarting_signal(SIGSEGV, SIG_DFL);
os_non_restarting_signal(SIGFPE, SIG_DFL);
os_non_restarting_signal(SIGILL, SIG_DFL);
os_non_restarting_signal(SIGBUS, SIG_DFL);

/* The length check is paranoia against some runaway code, and also because
(for a success return) lines in the spool file are read into big_buffer. */

if (local_scan_data != NULL)
  {
  int len = Ustrlen(local_scan_data);
  if (len > LOCAL_SCAN_MAX_RETURN) len = LOCAL_SCAN_MAX_RETURN;
  local_scan_data = string_copyn(local_scan_data, len);
  }

if (rc == LOCAL_SCAN_ACCEPT_FREEZE)
  {
  if (!deliver_freeze)         /* ACL might have already frozen */
    {
    deliver_freeze = TRUE;
    deliver_frozen_at = time(NULL);
    frozen_by = US"local_scan()";
    }
  rc = LOCAL_SCAN_ACCEPT;
  }
else if (rc == LOCAL_SCAN_ACCEPT_QUEUE)
  {
  if (!queue_only_policy)      /* ACL might have already queued */
    {
    queue_only_policy = TRUE;
    queued_by = US"local_scan()";
    }
  rc = LOCAL_SCAN_ACCEPT;
  }

/* Message accepted: remove newlines in local_scan_data because otherwise
the spool file gets corrupted. Ensure that all recipients are qualified. */

if (rc == LOCAL_SCAN_ACCEPT)
  {
  if (local_scan_data != NULL)
    {
    uschar *s;
    for (s = local_scan_data; *s != 0; s++) if (*s == '\n') *s = ' ';
    }
  for (i = 0; i < recipients_count; i++)
    {
    recipient_item *r = recipients_list + i;
    r->address = rewrite_address_qualify(r->address, TRUE);
    if (r->errors_to != NULL)
      r->errors_to = rewrite_address_qualify(r->errors_to, TRUE);
    }
  if (recipients_count == 0 && blackholed_by == NULL)
    blackholed_by = US"local_scan";
  }

/* Message rejected: newlines permitted in local_scan_data to generate
multiline SMTP responses. */

else
  {
  uschar *istemp = US"";
  uschar *smtp_code;
  gstring * g;

  errmsg = local_scan_data;

  Uunlink(spool_name);          /* Cancel this message */
  switch(rc)
    {
    default:
    log_write(0, LOG_MAIN, "invalid return %d from local_scan(). Temporary "
      "rejection given", rc);
    goto TEMPREJECT;

    case LOCAL_SCAN_REJECT_NOLOGHDR:
    BIT_CLEAR(log_selector, log_selector_size, Li_rejected_header);
    /* Fall through */

    case LOCAL_SCAN_REJECT:
    smtp_code = US"550";
    if (errmsg == NULL) errmsg =  US"Administrative prohibition";
    break;

    case LOCAL_SCAN_TEMPREJECT_NOLOGHDR:
    BIT_CLEAR(log_selector, log_selector_size, Li_rejected_header);
    /* Fall through */

    case LOCAL_SCAN_TEMPREJECT:
    TEMPREJECT:
    smtp_code = US"451";
    if (errmsg == NULL) errmsg = US"Temporary local problem";
    istemp = US"temporarily ";
    break;
    }

  g = string_append(g, 2, US"F=",
    sender_address[0] == 0 ? US"<>" : sender_address);
  g = add_host_info_for_log(g);

  log_write(0, LOG_MAIN|LOG_REJECT, "%s %srejected by local_scan(): %.256s",
    string_from_gstring(g), istemp, string_printing(errmsg));

  if (smtp_input)
    {
    if (!smtp_batched_input)
      {
      smtp_respond(smtp_code, 3, TRUE, errmsg);
      message_id[0] = 0;            /* Indicate no message accepted */
      smtp_reply = US"";            /* Indicate reply already sent */
      goto TIDYUP;                  /* Skip to end of function */
      }
    else
      {
      moan_smtp_batch(NULL, "%s %s", smtp_code, errmsg);
      /* Does not return */
      }
    }
  else
    {
    fseek(data_file, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);
    give_local_error(ERRMESS_LOCAL_SCAN, errmsg,
      US"message rejected by local scan code: ", error_rc, data_file,
        header_list);
    /* Does not return */
    }
  }

/* Reset signal handlers to ignore signals that previously would have caused
the message to be abandoned. */

signal(SIGTERM, SIG_IGN);
signal(SIGINT, SIG_IGN);


/* Ensure the first time flag is set in the newly-received message. */

deliver_firsttime = TRUE;

#ifdef EXPERIMENTAL_BRIGHTMAIL
if (bmi_run == 1)
  { /* rewind data file */
  lseek(data_fd, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);
  bmi_verdicts = bmi_process_message(header_list, data_fd);
  }
#endif

/* Update the timestamp in our Received: header to account for any time taken by
an ACL or by local_scan(). The new time is the time that all reception
processing is complete. */

timestamp = expand_string(US"${tod_full}");
tslen = Ustrlen(timestamp);

memcpy(received_header->text + received_header->slen - tslen - 1,
  timestamp, tslen);

/* In MUA wrapper mode, ignore queueing actions set by ACL or local_scan() */

if (mua_wrapper)
  {
  deliver_freeze = FALSE;
  queue_only_policy = FALSE;
  }

/* Keep the data file open until we have written the header file, in order to
hold onto the lock. In a -bh run, or if the message is to be blackholed, we
don't write the header file, and we unlink the data file. If writing the header
file fails, we have failed to accept this message. */

if (host_checking || blackholed_by != NULL)
  {
  header_line *h;
  Uunlink(spool_name);
  msg_size = 0;                                  /* Compute size for log line */
  for (h = header_list; h != NULL; h = h->next)
    if (h->type != '*') msg_size += h->slen;
  }

/* Write the -H file */

else
  if ((msg_size = spool_write_header(message_id, SW_RECEIVING, &errmsg)) < 0)
    {
    log_write(0, LOG_MAIN, "Message abandoned: %s", errmsg);
    Uunlink(spool_name);           /* Lose the data file */

    if (smtp_input)
      {
      smtp_reply = US"451 Error in writing spool file";
      message_id[0] = 0;          /* Indicate no message accepted */
      goto TIDYUP;
      }
    else
      {
      fseek(data_file, (long int)SPOOL_DATA_START_OFFSET, SEEK_SET);
      give_local_error(ERRMESS_IOERR, errmsg, US"", error_rc, data_file,
        header_list);
      /* Does not return */
      }
    }


/* The message has now been successfully received. */

receive_messagecount++;

/* In SMTP sessions we may receive several in one connection. After each one,
we wait for the clock to tick at the level of message-id granularity. This is
so that the combination of time+pid is unique, even on systems where the pid
can be re-used within our time interval. We can't shorten the interval without
re-designing the message-id. See comments above where the message id is
created. This is Something For The Future. */

message_id_tv.tv_usec = (message_id_tv.tv_usec/id_resolution) * id_resolution;
exim_wait_tick(&message_id_tv, id_resolution);

/* Add data size to written header size. We do not count the initial file name
that is in the file, but we do add one extra for the notional blank line that
precedes the data. This total differs from message_size in that it include the
added Received: header and any other headers that got created locally. */

fflush(data_file);
fstat(data_fd, &statbuf);

msg_size += statbuf.st_size - SPOOL_DATA_START_OFFSET + 1;

/* Generate a "message received" log entry. We do this by building up a dynamic
string as required. Since we commonly want to add two items at a time, use a
macro to simplify the coding. We log the arrival of a new message while the
file is still locked, just in case the machine is *really* fast, and delivers
it first! Include any message id that is in the message - since the syntax of a
message id is actually an addr-spec, we can use the parse routine to canonicalize
it. */

g = string_get(256);

g = string_append(g, 2,
  fake_response == FAIL ? US"(= " : US"<= ",
  sender_address[0] == 0 ? US"<>" : sender_address);
if (message_reference)
  g = string_append(g, 2, US" R=", message_reference);

g = add_host_info_for_log(g);

#ifdef SUPPORT_TLS
if (LOGGING(tls_cipher) && tls_in.cipher)
  g = string_append(g, 2, US" X=", tls_in.cipher);
if (LOGGING(tls_certificate_verified) && tls_in.cipher)
  g = string_append(g, 2, US" CV=", tls_in.certificate_verified ? "yes":"no");
if (LOGGING(tls_peerdn) && tls_in.peerdn)
  g = string_append(g, 3, US" DN=\"", string_printing(tls_in.peerdn), US"\"");
if (LOGGING(tls_sni) && tls_in.sni)
  g = string_append(g, 3, US" SNI=\"", string_printing(tls_in.sni), US"\"");
#endif

if (sender_host_authenticated)
  {
  g = string_append(g, 2, US" A=", sender_host_authenticated);
  if (authenticated_id)
    {
    g = string_append(g, 2, US":", authenticated_id);
    if (LOGGING(smtp_mailauth) && authenticated_sender)
      g = string_append(g, 2, US":", authenticated_sender);
    }
  }

#ifndef DISABLE_PRDR
if (prdr_requested)
  g = string_catn(g, US" PRDR", 5);
#endif

#ifdef SUPPORT_PROXY
if (proxy_session && LOGGING(proxy))
  g = string_append(g, 2, US" PRX=", proxy_local_address);
#endif

if (chunking_state > CHUNKING_OFFERED)
  g = string_catn(g, US" K", 2);

sprintf(CS big_buffer, "%d", msg_size);
g = string_append(g, 2, US" S=", big_buffer);

/* log 8BITMIME mode announced in MAIL_FROM
   0 ... no BODY= used
   7 ... 7BIT
   8 ... 8BITMIME */
if (LOGGING(8bitmime))
  {
  sprintf(CS big_buffer, "%d", body_8bitmime);
  g = string_append(g, 2, US" M8S=", big_buffer);
  }

if (*queue_name)
  g = string_append(g, 2, US" Q=", queue_name);

/* If an addr-spec in a message-id contains a quoted string, it can contain
any characters except " \ and CR and so in particular it can contain NL!
Therefore, make sure we use a printing-characters only version for the log.
Also, allow for domain literals in the message id. */

if (msgid_header)
  {
  uschar *old_id;
  BOOL save_allow_domain_literals = allow_domain_literals;
  allow_domain_literals = TRUE;
  old_id = parse_extract_address(Ustrchr(msgid_header->text, ':') + 1,
    &errmsg, &start, &end, &domain, FALSE);
  allow_domain_literals = save_allow_domain_literals;
  if (old_id != NULL)
    g = string_append(g, 2, US" id=", string_printing(old_id));
  }

/* If subject logging is turned on, create suitable printing-character
text. By expanding $h_subject: we make use of the MIME decoding. */

if (LOGGING(subject) && subject_header != NULL)
  {
  int i;
  uschar *p = big_buffer;
  uschar *ss = expand_string(US"$h_subject:");

  /* Backslash-quote any double quotes or backslashes so as to make a
  a C-like string, and turn any non-printers into escape sequences. */

  *p++ = '\"';
  if (*ss != 0) for (i = 0; i < 100 && ss[i] != 0; i++)
    {
    if (ss[i] == '\"' || ss[i] == '\\') *p++ = '\\';
    *p++ = ss[i];
    }
  *p++ = '\"';
  *p = 0;
  g = string_append(g, 2, US" T=", string_printing(big_buffer));
  }

/* Terminate the string: string_cat() and string_append() leave room, but do
not put the zero in. */

(void) string_from_gstring(g);

/* Create a message log file if message logs are being used and this message is
not blackholed. Write the reception stuff to it. We used to leave message log
creation until the first delivery, but this has proved confusing for some
people. */

if (message_logs && !blackholed_by)
  {
  int fd;

  spool_name = spool_fname(US"msglog", message_subdir, message_id, US"");
  
  if (  (fd = Uopen(spool_name, O_WRONLY|O_APPEND|O_CREAT, SPOOL_MODE)) < 0
     && errno == ENOENT
     )
    {
    (void)directory_make(spool_directory,
			spool_sname(US"msglog", message_subdir),
			MSGLOG_DIRECTORY_MODE, TRUE);
    fd = Uopen(spool_name, O_WRONLY|O_APPEND|O_CREAT, SPOOL_MODE);
    }

  if (fd < 0)
    log_write(0, LOG_MAIN|LOG_PANIC, "Couldn't open message log %s: %s",
      spool_name, strerror(errno));
  else
    {
    FILE *message_log = fdopen(fd, "a");
    if (message_log == NULL)
      {
      log_write(0, LOG_MAIN|LOG_PANIC, "Couldn't fdopen message log %s: %s",
        spool_name, strerror(errno));
      (void)close(fd);
      }
    else
      {
      uschar *now = tod_stamp(tod_log);
      fprintf(message_log, "%s Received from %s\n", now, g->s+3);
      if (deliver_freeze) fprintf(message_log, "%s frozen by %s\n", now,
        frozen_by);
      if (queue_only_policy) fprintf(message_log,
        "%s no immediate delivery: queued%s%s by %s\n", now,
        *queue_name ? " in " : "", *queue_name ? CS queue_name : "",
	queued_by);
      (void)fclose(message_log);
      }
    }
  }

/* Everything has now been done for a successful message except logging its
arrival, and outputting an SMTP response. While writing to the log, set a flag
to cause a call to receive_bomb_out() if the log cannot be opened. */

receive_call_bombout = TRUE;

/* Before sending an SMTP response in a TCP/IP session, we check to see if the
connection has gone away. This can only be done if there is no unconsumed input
waiting in the local input buffer. We can test for this by calling
receive_smtp_buffered(). RFC 2920 (pipelining) explicitly allows for additional
input to be sent following the final dot, so the presence of following input is
not an error.

If the connection is still present, but there is no unread input for the
socket, the result of a select() call will be zero. If, however, the connection
has gone away, or if there is pending input, the result of select() will be
non-zero. The two cases can be distinguished by trying to read the next input
character. If we succeed, we can unread it so that it remains in the local
buffer for handling later. If not, the connection has been lost.

Of course, since TCP/IP is asynchronous, there is always a chance that the
connection will vanish between the time of this test and the sending of the
response, but the chance of this happening should be small. */

if (smtp_input && sender_host_address != NULL && !sender_host_notsocket &&
    !receive_smtp_buffered())
  {
  struct timeval tv;
  fd_set select_check;
  FD_ZERO(&select_check);
  FD_SET(fileno(smtp_in), &select_check);
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  if (select(fileno(smtp_in) + 1, &select_check, NULL, NULL, &tv) != 0)
    {
    int c = (receive_getc)(GETC_BUFFER_UNLIMITED);
    if (c != EOF) (receive_ungetc)(c); else
      {
      smtp_notquit_exit(US"connection-lost", NULL, NULL);
      smtp_reply = US"";    /* No attempt to send a response */
      smtp_yield = FALSE;   /* Nothing more on this connection */

      /* Re-use the log line workspace */

      g->ptr = 0;
      g = string_cat(g, US"SMTP connection lost after final dot");
      g = add_host_info_for_log(g);
      log_write(0, LOG_MAIN, "%s", string_from_gstring(g));

      /* Delete the files for this aborted message. */

      Uunlink(spool_fname(US"input", message_subdir, message_id, US"-D"));
      Uunlink(spool_fname(US"input", message_subdir, message_id, US"-H"));
      Uunlink(spool_fname(US"msglog", message_subdir, message_id, US""));

      goto TIDYUP;
      }
    }
  }

/* The connection has not gone away; we really are going to take responsibility
for this message. */

/* Cutthrough - had sender last-dot; assume we've sent (or bufferred) all
   data onward by now.

   Send dot onward.  If accepted, wipe the spooled files, log as delivered and accept
   the sender's dot (below).
   If rejected: copy response to sender, wipe the spooled files, log appropriately.
   If temp-reject: normally accept to sender, keep the spooled file - unless defer=pass
   in which case pass temp-reject back to initiator and dump the files.

   Having the normal spool files lets us do data-filtering, and store/forward on temp-reject.

   XXX We do not handle queue-only, freezing, or blackholes.
*/
if(cutthrough.fd >= 0 && cutthrough.delivery)
  {
  uschar * msg = cutthrough_finaldot();	/* Ask the target system to accept the message */
					/* Logging was done in finaldot() */
  switch(msg[0])
    {
    case '2':	/* Accept. Do the same to the source; dump any spoolfiles.   */
      cutthrough_done = ACCEPTED;
      break;					/* message_id needed for SMTP accept below */

    case '4':	/* Temp-reject. Keep spoolfiles and accept, unless defer-pass mode.
      		... for which, pass back the exact error */
      if (cutthrough.defer_pass) smtp_reply = string_copy_malloc(msg);
      /*FALLTRHOUGH*/

    default:	/* Unknown response, or error.  Treat as temp-reject.         */
      cutthrough_done = TMP_REJ;		/* Avoid the usual immediate delivery attempt */
      break;					/* message_id needed for SMTP accept below */

    case '5':	/* Perm-reject.  Do the same to the source.  Dump any spoolfiles */
      smtp_reply = string_copy_malloc(msg);		/* Pass on the exact error */
      cutthrough_done = PERM_REJ;
      break;
    }
  }

#ifndef DISABLE_PRDR
if(!smtp_reply || prdr_requested)
#else
if(!smtp_reply)
#endif
  {
  log_write(0, LOG_MAIN |
    (LOGGING(received_recipients)? LOG_RECIPIENTS : 0) |
    (LOGGING(received_sender)? LOG_SENDER : 0),
    "%s", g->s);

  /* Log any control actions taken by an ACL or local_scan(). */

  if (deliver_freeze) log_write(0, LOG_MAIN, "frozen by %s", frozen_by);
  if (queue_only_policy) log_write(L_delay_delivery, LOG_MAIN,
    "no immediate delivery: queued%s%s by %s",
    *queue_name ? " in " : "", *queue_name ? CS queue_name : "",       
    queued_by);
  }
receive_call_bombout = FALSE;

store_reset(g);   /* The store for the main log message can be reused */

/* If the message is frozen, and freeze_tell is set, do the telling. */

if (deliver_freeze && freeze_tell != NULL && freeze_tell[0] != 0)
  {
  moan_tell_someone(freeze_tell, NULL, US"Message frozen on arrival",
    "Message %s was frozen on arrival by %s.\nThe sender is <%s>.\n",
    message_id, frozen_by, sender_address);
  }


/* Either a message has been successfully received and written to the two spool
files, or an error in writing the spool has occurred for an SMTP message, or
an SMTP message has been rejected for policy reasons. (For a non-SMTP message
we will have already given up because there's no point in carrying on!) In
either event, we must now close (and thereby unlock) the data file. In the
successful case, this leaves the message on the spool, ready for delivery. In
the error case, the spool file will be deleted. Then tidy up store, interact
with an SMTP call if necessary, and return.

A fflush() was done earlier in the expectation that any write errors on the
data file will be flushed(!) out thereby. Nevertheless, it is theoretically
possible for fclose() to fail - but what to do? What has happened to the lock
if this happens? */


TIDYUP:
process_info[process_info_len] = 0;                /* Remove message id */
if (data_file != NULL) (void)fclose(data_file);    /* Frees the lock */

/* Now reset signal handlers to their defaults */

signal(SIGTERM, SIG_DFL);
signal(SIGINT, SIG_DFL);

/* Tell an SMTP caller the state of play, and arrange to return the SMTP return
value, which defaults TRUE - meaning there may be more incoming messages from
this connection. For non-SMTP callers (where there is only ever one message),
the default is FALSE. */

if (smtp_input)
  {
  yield = smtp_yield;

  /* Handle interactive SMTP callers. After several kinds of error, smtp_reply
  is set to the response that should be sent. When it is NULL, we generate
  default responses. After an ACL error or local_scan() error, the response has
  already been sent, and smtp_reply is an empty string to indicate this. */

  if (!smtp_batched_input)
    {
    if (!smtp_reply)
      {
      if (fake_response != OK)
        smtp_respond(fake_response == DEFER ? US"450" : US"550",
	  3, TRUE, fake_response_text);

      /* An OK response is required; use "message" text if present. */

      else if (user_msg)
        {
        uschar *code = US"250";
        int len = 3;
        smtp_message_code(&code, &len, &user_msg, NULL, TRUE);
        smtp_respond(code, len, TRUE, user_msg);
        }

      /* Default OK response */

      else if (chunking_state > CHUNKING_OFFERED)
	{
        smtp_printf("250- %u byte chunk, total %d\r\n250 OK id=%s\r\n", FALSE,
	    chunking_datasize, message_size+message_linecount, message_id);
	chunking_state = CHUNKING_OFFERED;
	}
      else
        smtp_printf("250 OK id=%s\r\n", FALSE, message_id);

      if (host_checking)
        fprintf(stdout,
          "\n**** SMTP testing: that is not a real message id!\n\n");
      }

    /* smtp_reply is set non-empty */

    else if (smtp_reply[0] != 0)
      if (fake_response != OK && (smtp_reply[0] == '2'))
        smtp_respond((fake_response == DEFER)? US"450" : US"550", 3, TRUE,
          fake_response_text);
      else
        smtp_printf("%.1024s\r\n", FALSE, smtp_reply);

    switch (cutthrough_done)
      {
      case ACCEPTED:
	log_write(0, LOG_MAIN, "Completed");/* Delivery was done */
      case PERM_REJ:
							 /* Delete spool files */
	Uunlink(spool_fname(US"input", message_subdir, message_id, US"-D"));
	Uunlink(spool_fname(US"input", message_subdir, message_id, US"-H"));
	Uunlink(spool_fname(US"msglog", message_subdir, message_id, US""));
	break;

      case TMP_REJ:
	if (cutthrough.defer_pass)
	  {
	  Uunlink(spool_fname(US"input", message_subdir, message_id, US"-D"));
	  Uunlink(spool_fname(US"input", message_subdir, message_id, US"-H"));
	  Uunlink(spool_fname(US"msglog", message_subdir, message_id, US""));
	  }
      default:
	break;
      }
    if (cutthrough_done != NOT_TRIED)
      {
      message_id[0] = 0;	  /* Prevent a delivery from starting */
      cutthrough.delivery = cutthrough.callout_hold_only = FALSE;
      cutthrough.defer_pass = FALSE;
      }
    }

  /* For batched SMTP, generate an error message on failure, and do
  nothing on success. The function moan_smtp_batch() does not return -
  it exits from the program with a non-zero return code. */

  else if (smtp_reply)
    moan_smtp_batch(NULL, "%s", smtp_reply);
  }


/* If blackholing, we can immediately log this message's sad fate. The data
file has already been unlinked, and the header file was never written to disk.
We must now indicate that nothing was received, to prevent a delivery from
starting. */

if (blackholed_by)
  {
  const uschar *detail = local_scan_data
    ? string_printing(local_scan_data)
    : string_sprintf("(%s discarded recipients)", blackholed_by);
  log_write(0, LOG_MAIN, "=> blackhole %s%s", detail, blackhole_log_msg);
  log_write(0, LOG_MAIN, "Completed");
  message_id[0] = 0;
  }

/* Reset headers so that logging of rejects for a subsequent message doesn't
include them. It is also important to set header_last = NULL before exiting
from this function, as this prevents certain rewrites that might happen during
subsequent verifying (of another incoming message) from trying to add headers
when they shouldn't. */

header_list = header_last = NULL;

return yield;  /* TRUE if more messages (SMTP only) */
}