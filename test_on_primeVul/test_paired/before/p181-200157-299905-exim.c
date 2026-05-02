readconf_main(void)
{
int sep = 0;
struct stat statbuf;
uschar *s, *filename;
uschar *list = config_main_filelist;

/* Loop through the possible file names */

while((filename = string_nextinlist(&list, &sep, big_buffer, big_buffer_size))
       != NULL)
  {
  /* Cut out all the fancy processing unless specifically wanted */

  #if defined(CONFIGURE_FILE_USE_NODE) || defined(CONFIGURE_FILE_USE_EUID)
  uschar *suffix = filename + Ustrlen(filename);

  /* Try for the node-specific file if a node name exists */

  #ifdef CONFIGURE_FILE_USE_NODE
  struct utsname uts;
  if (uname(&uts) >= 0)
    {
    #ifdef CONFIGURE_FILE_USE_EUID
    sprintf(CS suffix, ".%ld.%.256s", (long int)original_euid, uts.nodename);
    config_file = Ufopen(filename, "rb");
    if (config_file == NULL)
    #endif  /* CONFIGURE_FILE_USE_EUID */
      {
      sprintf(CS suffix, ".%.256s", uts.nodename);
      config_file = Ufopen(filename, "rb");
      }
    }
  #endif  /* CONFIGURE_FILE_USE_NODE */

  /* Otherwise, try the generic name, possibly with the euid added */

  #ifdef CONFIGURE_FILE_USE_EUID
  if (config_file == NULL)
    {
    sprintf(CS suffix, ".%ld", (long int)original_euid);
    config_file = Ufopen(filename, "rb");
    }
  #endif  /* CONFIGURE_FILE_USE_EUID */

  /* Finally, try the unadorned name */

  if (config_file == NULL)
    {
    *suffix = 0;
    config_file = Ufopen(filename, "rb");
    }
  #else  /* if neither defined */

  /* This is the common case when the fancy processing is not included. */

  config_file = Ufopen(filename, "rb");
  #endif

  /* If the file does not exist, continue to try any others. For any other
  error, break out (and die). */

  if (config_file != NULL || errno != ENOENT) break;
  }

/* On success, save the name for verification; config_filename is used when
logging configuration errors (it changes for .included files) whereas
config_main_filename is the name shown by -bP. Failure to open a configuration
file is a serious disaster. */

if (config_file != NULL)
  {
  config_filename = config_main_filename = string_copy(filename);
  }
else
  {
  if (filename == NULL)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "non-existent configuration file(s): "
      "%s", config_main_filelist);
  else
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "%s", string_open_failed(errno,
      "configuration file %s", filename));
  }

/* Check the status of the file we have opened, unless it was specified on
the command line, in which case privilege was given away at the start. */

if (!config_changed)
  {
  if (fstat(fileno(config_file), &statbuf) != 0)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to stat configuration file %s",
      big_buffer);

  if ((statbuf.st_uid != root_uid                /* owner not root */
       #ifdef CONFIGURE_OWNER
       && statbuf.st_uid != config_uid           /* owner not the special one */
       #endif
         ) ||                                    /* or */
      (statbuf.st_gid != root_gid                /* group not root & */
       #ifdef CONFIGURE_GROUP
       && statbuf.st_gid != config_gid           /* group not the special one */
       #endif
       && (statbuf.st_mode & 020) != 0) ||       /* group writeable  */
                                                 /* or */
      ((statbuf.st_mode & 2) != 0))              /* world writeable  */

    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "Exim configuration file %s has the "
      "wrong owner, group, or mode", big_buffer);
  }

/* Process the main configuration settings. They all begin with a lower case
letter. If we see something starting with an upper case letter, it is taken as
a macro definition. */

while ((s = get_config_line()) != NULL)
  {
  if (isupper(s[0])) read_macro_assignment(s);

  else if (Ustrncmp(s, "domainlist", 10) == 0)
    read_named_list(&domainlist_anchor, &domainlist_count,
      MAX_NAMED_LIST, s+10, US"domain list");

  else if (Ustrncmp(s, "hostlist", 8) == 0)
    read_named_list(&hostlist_anchor, &hostlist_count,
      MAX_NAMED_LIST, s+8, US"host list");

  else if (Ustrncmp(s, US"addresslist", 11) == 0)
    read_named_list(&addresslist_anchor, &addresslist_count,
      MAX_NAMED_LIST, s+11, US"address list");

  else if (Ustrncmp(s, US"localpartlist", 13) == 0)
    read_named_list(&localpartlist_anchor, &localpartlist_count,
      MAX_NAMED_LIST, s+13, US"local part list");

  else
    (void) readconf_handle_option(s, optionlist_config, optionlist_config_size,
      NULL, US"main option \"%s\" unknown");
  }


/* If local_sender_retain is set, local_from_check must be unset. */

if (local_sender_retain && local_from_check)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE, "both local_from_check and "
    "local_sender_retain are set; this combination is not allowed");

/* If the timezone string is empty, set it to NULL, implying no TZ variable
wanted. */

if (timezone_string != NULL && *timezone_string == 0) timezone_string = NULL;

/* The max retry interval must not be greater than 24 hours. */

if (retry_interval_max > 24*60*60) retry_interval_max = 24*60*60;

/* remote_max_parallel must be > 0 */

if (remote_max_parallel <= 0) remote_max_parallel = 1;

/* Save the configured setting of freeze_tell, so we can re-instate it at the
start of a new SMTP message. */

freeze_tell_config = freeze_tell;

/* The primary host name may be required for expansion of spool_directory
and log_file_path, so make sure it is set asap. It is obtained from uname(),
but if that yields an unqualified value, make a FQDN by using gethostbyname to
canonize it. Some people like upper case letters in their host names, so we
don't force the case. */

if (primary_hostname == NULL)
  {
  uschar *hostname;
  struct utsname uts;
  if (uname(&uts) < 0)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "uname() failed to yield host name");
  hostname = US uts.nodename;

  if (Ustrchr(hostname, '.') == NULL)
    {
    int af = AF_INET;
    struct hostent *hostdata;

    #if HAVE_IPV6
    if (!disable_ipv6 && (dns_ipv4_lookup == NULL ||
         match_isinlist(hostname, &dns_ipv4_lookup, 0, NULL, NULL, MCL_DOMAIN,
           TRUE, NULL) != OK))
      af = AF_INET6;
    #else
    af = AF_INET;
    #endif

    for (;;)
      {
      #if HAVE_IPV6
        #if HAVE_GETIPNODEBYNAME
        int error_num;
        hostdata = getipnodebyname(CS hostname, af, 0, &error_num);
        #else
        hostdata = gethostbyname2(CS hostname, af);
        #endif
      #else
      hostdata = gethostbyname(CS hostname);
      #endif

      if (hostdata != NULL)
        {
        hostname = US hostdata->h_name;
        break;
        }

      if (af == AF_INET) break;
      af = AF_INET;
      }
    }

  primary_hostname = string_copy(hostname);
  }

/* Set up default value for smtp_active_hostname */

smtp_active_hostname = primary_hostname;

/* If spool_directory wasn't set in the build-time configuration, it must have
got set above. Of course, writing to the log may not work if log_file_path is
not set, but it will at least get to syslog or somewhere, with any luck. */

if (*spool_directory == 0)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE, "spool_directory undefined: cannot "
    "proceed");

/* Expand the spool directory name; it may, for example, contain the primary
host name. Same comment about failure. */

s = expand_string(spool_directory);
if (s == NULL)
  log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to expand spool_directory "
    "\"%s\": %s", spool_directory, expand_string_message);
spool_directory = s;

/* Expand log_file_path, which must contain "%s" in any component that isn't
the null string or "syslog". It is also allowed to contain one instance of %D.
However, it must NOT contain % followed by anything else. */

if (*log_file_path != 0)
  {
  uschar *ss, *sss;
  int sep = ':';                       /* Fixed for log file path */
  s = expand_string(log_file_path);
  if (s == NULL)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to expand log_file_path "
      "\"%s\": %s", log_file_path, expand_string_message);

  ss = s;
  while ((sss = string_nextinlist(&ss,&sep,big_buffer,big_buffer_size)) != NULL)
    {
    uschar *t;
    if (sss[0] == 0 || Ustrcmp(sss, "syslog") == 0) continue;
    t = Ustrstr(sss, "%s");
    if (t == NULL)
      log_write(0, LOG_MAIN|LOG_PANIC_DIE, "log_file_path \"%s\" does not "
        "contain \"%%s\"", sss);
    *t = 'X';
    t = Ustrchr(sss, '%');
    if (t != NULL)
      {
      if (t[1] != 'D' || Ustrchr(t+2, '%') != NULL)
        log_write(0, LOG_MAIN|LOG_PANIC_DIE, "log_file_path \"%s\" contains "
          "unexpected \"%%\" character", s);
      }
    }

  log_file_path = s;
  }

/* Interpret syslog_facility into an integer argument for 'ident' param to
openlog(). Default is LOG_MAIL set in globals.c. Allow the user to omit the
leading "log_". */

if (syslog_facility_str != NULL)
  {
  int i;
  uschar *s = syslog_facility_str;

  if ((Ustrlen(syslog_facility_str) >= 4) &&
        (strncmpic(syslog_facility_str, US"log_", 4) == 0))
    s += 4;

  for (i = 0; i < syslog_list_size; i++)
    {
    if (strcmpic(s, syslog_list[i].name) == 0)
      {
      syslog_facility = syslog_list[i].value;
      break;
      }
    }

  if (i >= syslog_list_size)
    {
    log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
      "failed to interpret syslog_facility \"%s\"", syslog_facility_str);
    }
  }

/* Expand pid_file_path */

if (*pid_file_path != 0)
  {
  s = expand_string(pid_file_path);
  if (s == NULL)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "failed to expand pid_file_path "
      "\"%s\": %s", pid_file_path, expand_string_message);
  pid_file_path = s;
  }

/* Compile the regex for matching a UUCP-style "From_" line in an incoming
message. */

regex_From = regex_must_compile(uucp_from_pattern, FALSE, TRUE);

/* Unpick the SMTP rate limiting options, if set */

if (smtp_ratelimit_mail != NULL)
  {
  unpick_ratelimit(smtp_ratelimit_mail, &smtp_rlm_threshold,
    &smtp_rlm_base, &smtp_rlm_factor, &smtp_rlm_limit);
  }

if (smtp_ratelimit_rcpt != NULL)
  {
  unpick_ratelimit(smtp_ratelimit_rcpt, &smtp_rlr_threshold,
    &smtp_rlr_base, &smtp_rlr_factor, &smtp_rlr_limit);
  }

/* The qualify domains default to the primary host name */

if (qualify_domain_sender == NULL)
  qualify_domain_sender = primary_hostname;
if (qualify_domain_recipient == NULL)
  qualify_domain_recipient = qualify_domain_sender;

/* Setting system_filter_user in the configuration sets the gid as well if a
name is given, but a numerical value does not. */

if (system_filter_uid_set && !system_filter_gid_set)
  {
  struct passwd *pw = getpwuid(system_filter_uid);
  if (pw == NULL)
    log_write(0, LOG_MAIN|LOG_PANIC_DIE, "Failed to look up uid %ld",
      (long int)system_filter_uid);
  system_filter_gid = pw->pw_gid;
  system_filter_gid_set = TRUE;
  }

/* If the errors_reply_to field is set, check that it is syntactically valid
and ensure it contains a domain. */

if (errors_reply_to != NULL)
  {
  uschar *errmess;
  int start, end, domain;
  uschar *recipient = parse_extract_address(errors_reply_to, &errmess,
    &start, &end, &domain, FALSE);

  if (recipient == NULL)
    log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
      "error in errors_reply_to (%s): %s", errors_reply_to, errmess);

  if (domain == 0)
    log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
      "errors_reply_to (%s) does not contain a domain", errors_reply_to);
  }

/* If smtp_accept_queue or smtp_accept_max_per_host is set, then
smtp_accept_max must also be set. */

if (smtp_accept_max == 0 &&
    (smtp_accept_queue > 0 || smtp_accept_max_per_host != NULL))
  log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
    "smtp_accept_max must be set if smtp_accept_queue or "
    "smtp_accept_max_per_host is set");

/* Set up the host number if anything is specified. It is an expanded string
so that it can be computed from the host name, for example. We do this last
so as to ensure that everything else is set up before the expansion. */

if (host_number_string != NULL)
  {
  uschar *end;
  uschar *s = expand_string(host_number_string);
  long int n = Ustrtol(s, &end, 0);
  while (isspace(*end)) end++;
  if (*end != 0)
    log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
      "localhost_number value is not a number: %s", s);
  if (n > LOCALHOST_MAX)
    log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
      "localhost_number is greater than the maximum allowed value (%d)",
        LOCALHOST_MAX);
  host_number = n;
  }

#ifdef SUPPORT_TLS
/* If tls_verify_hosts is set, tls_verify_certificates must also be set */

if ((tls_verify_hosts != NULL || tls_try_verify_hosts != NULL) &&
     tls_verify_certificates == NULL)
  log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
    "tls_%sverify_hosts is set, but tls_verify_certificates is not set",
    (tls_verify_hosts != NULL)? "" : "try_");

/* If openssl_options is set, validate it */
if (openssl_options != NULL)
  {
# ifdef USE_GNUTLS
  log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
    "openssl_options is set but we're using GnuTLS");
# else
  long dummy;
  if (!(tls_openssl_options_parse(openssl_options, &dummy)))
    log_write(0, LOG_PANIC_DIE|LOG_CONFIG,
      "openssl_options parse error: %s", openssl_options);
# endif
  }
#endif
}