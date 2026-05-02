static void fix_hostname(struct SessionHandle *data,
                         struct connectdata *conn, struct hostname *host)
{
  size_t len;

#ifndef USE_LIBIDN
  (void)data;
  (void)conn;
#elif defined(CURL_DISABLE_VERBOSE_STRINGS)
  (void)conn;
#endif

  /* set the name we use to display the host name */
  host->dispname = host->name;

  len = strlen(host->name);
  if(host->name[len-1] == '.')
    /* strip off a single trailing dot if present, primarily for SNI but
       there's no use for it */
    host->name[len-1]=0;

  if(!is_ASCII_name(host->name)) {
#ifdef USE_LIBIDN
  /*************************************************************
   * Check name for non-ASCII and convert hostname to ACE form.
   *************************************************************/
  if(stringprep_check_version(LIBIDN_REQUIRED_VERSION)) {
    char *ace_hostname = NULL;
    int rc = idna_to_ascii_lz(host->name, &ace_hostname, 0);
    infof (data, "Input domain encoded as `%s'\n",
           stringprep_locale_charset ());
    if(rc != IDNA_SUCCESS)
      infof(data, "Failed to convert %s to ACE; %s\n",
            host->name, Curl_idn_strerror(conn, rc));
    else {
      /* tld_check_name() displays a warning if the host name contains
         "illegal" characters for this TLD */
      (void)tld_check_name(data, ace_hostname);

      host->encalloc = ace_hostname;
      /* change the name pointer to point to the encoded hostname */
      host->name = host->encalloc;
    }
  }
#elif defined(USE_WIN32_IDN)
  /*************************************************************
   * Check name for non-ASCII and convert hostname to ACE form.
   *************************************************************/
    char *ace_hostname = NULL;
    int rc = curl_win32_idn_to_ascii(host->name, &ace_hostname);
    if(rc == 0)
      infof(data, "Failed to convert %s to ACE;\n",
            host->name);
    else {
      host->encalloc = ace_hostname;
      /* change the name pointer to point to the encoded hostname */
      host->name = host->encalloc;
    }
#else
    infof(data, "IDN support not present, can't parse Unicode domains\n");
#endif
  }
}