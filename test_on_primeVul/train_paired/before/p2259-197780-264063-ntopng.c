HTTPserver::HTTPserver(const char *_docs_dir, const char *_scripts_dir) {
  struct mg_callbacks callbacks;
  static char ports[256], ssl_cert_path[MAX_PATH] = { 0 }, access_log_path[MAX_PATH] = { 0 };
  const char *http_binding_addr = ntop->getPrefs()->get_http_binding_address();
  const char *https_binding_addr = ntop->getPrefs()->get_https_binding_address();
  char tmpBuf[8];
  bool use_ssl = false;
  bool use_http = true;
  struct stat statsBuf;
  int stat_rc;

  static char *http_options[] = {
    (char*)"listening_ports", ports,
    (char*)"enable_directory_listing", (char*)"no",
    (char*)"document_root",  (char*)_docs_dir,
    /* (char*)"extra_mime_types", (char*)"" */ /* see mongoose.c */
    (char*)"num_threads", (char*)"5",
    NULL, NULL, NULL, NULL,
    NULL
  };

  docs_dir = strdup(_docs_dir), scripts_dir = strdup(_scripts_dir);
  httpserver = this;
  if(ntop->getPrefs()->get_http_port() == 0) use_http = false;

  if(use_http) {
    snprintf(ports, sizeof(ports), "%s%s%d",
	     http_binding_addr,
	     (http_binding_addr[0] == '\0') ? "" : ":",
	     ntop->getPrefs()->get_http_port());
  }

  snprintf(ssl_cert_path, sizeof(ssl_cert_path), "%s/ssl/%s",
	   docs_dir, CONST_HTTPS_CERT_NAME);

  stat_rc = stat(ssl_cert_path, &statsBuf);

  if((ntop->getPrefs()->get_https_port() > 0) && (stat_rc == 0)) {
    int i;

    use_ssl = true;
    if(use_http)
      snprintf(ports, sizeof(ports), "%s%s%d,%s%s%ds",
	       http_binding_addr,
	       (http_binding_addr[0] == '\0') ? "" : ":",
	       ntop->getPrefs()->get_http_port(),
	       https_binding_addr,
	       (https_binding_addr[0] == '\0') ? "" : ":",
	       ntop->getPrefs()->get_https_port());
    else
      snprintf(ports, sizeof(ports), "%s%s%ds",
	       https_binding_addr,
	       (https_binding_addr[0] == '\0') ? "" : ":",
	       ntop->getPrefs()->get_https_port());

    ntop->getTrace()->traceEvent(TRACE_INFO, "Found SSL certificate %s", ssl_cert_path);

    for(i=0; http_options[i] != NULL; i++) ;

    http_options[i] = (char*)"ssl_certificate", http_options[i+1] = ssl_cert_path;
    ssl_enabled = true;
  } else {
    if(stat_rc != 0)
      ntop->getTrace()->traceEvent(TRACE_NORMAL,
				   "HTTPS Disabled: missing SSL certificate %s", ssl_cert_path);
    ntop->getTrace()->traceEvent(TRACE_NORMAL,
				 "Please read https://github.com/ntop/ntopng/blob/dev/doc/README.SSL if you want to enable SSL.");
    ssl_enabled = false;
  }

  /* Alternate HTTP port (required for Captive Portal) */
  if(use_http && ntop->getPrefs()->get_alt_http_port()) {
    snprintf(&ports[strlen(ports)], sizeof(ports) - strlen(ports) - 1, ",%s%s%d",
	     http_binding_addr,
	     (http_binding_addr[0] == '\0') ? "" : ":",
	     ntop->getPrefs()->get_alt_http_port());
  }

  if((!use_http) && (!use_ssl) & (!ssl_enabled)) {
    if(stat_rc != 0)
      ntop->getTrace()->traceEvent(TRACE_WARNING,
				   "Unable to start HTTP server: HTTP is disabled and the SSL certificate is missing.");
    ntop->getTrace()->traceEvent(TRACE_WARNING,
				 "Starting the HTTP server on the default port");
    snprintf(ports, sizeof(ports), "%d", ntop->getPrefs()->get_http_port());
    use_http = true;
  }

  ntop->getRedis()->get((char*)SPLASH_HTTP_PORT, tmpBuf, sizeof(tmpBuf), true);
  if(tmpBuf[0] != '\0') {
    http_splash_port = atoi(tmpBuf);

    if(http_splash_port > 0) {
      snprintf(&ports[strlen(ports)], sizeof(ports) - strlen(ports) - 1, ",%s%s%d",
	       http_binding_addr,
	       (http_binding_addr[0] == '\0') ? "" : ":",
	       http_splash_port);

      /* Mongoose uses network byte order */
      http_splash_port = ntohs(http_splash_port);
    } else
      ntop->getTrace()->traceEvent(TRACE_WARNING, "Ignoring HTTP splash port (%s)", tmpBuf);
  } else
    http_splash_port = 0;

  if(ntop->getPrefs()->is_access_log_enabled()) {
    int i;

    snprintf(access_log_path, sizeof(access_log_path), "%s/ntopng_access.log",
	     ntop->get_working_dir());

    for(i=0; http_options[i] != NULL; i++)
      ;

    http_options[i] = (char*)"access_log_file", http_options[i+1] = access_log_path;
    ntop->getTrace()->traceEvent(TRACE_NORMAL, "HTTP logs will be stored on %s", access_log_path);
  }

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.begin_request = handle_lua_request;
  callbacks.log_message = handle_http_message;

  /* mongoose */
  http_prefix = ntop->getPrefs()->get_http_prefix(),
    http_prefix_len = strlen(ntop->getPrefs()->get_http_prefix());

  httpd_v4 = mg_start(&callbacks, NULL, (const char**)http_options);

  if(httpd_v4 == NULL) {
    ntop->getTrace()->traceEvent(TRACE_ERROR, "Unable to start HTTP server (IPv4) on ports %s", ports);
    if (errno)
      ntop->getTrace()->traceEvent(TRACE_ERROR, "%s", strerror(errno));
    exit(-1);
  }

  /* ***************************** */

  ntop->getTrace()->traceEvent(TRACE_NORMAL, "Web server dirs [%s][%s]", docs_dir, scripts_dir);

  if(use_http)
    ntop->getTrace()->traceEvent(TRACE_NORMAL, "HTTP server listening on port(s) %s",
				 ports);

  if(use_ssl & ssl_enabled)
    ntop->getTrace()->traceEvent(TRACE_NORMAL, "HTTPS server listening on port %d",
				 ntop->getPrefs()->get_https_port());
};