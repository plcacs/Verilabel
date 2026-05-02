apr_status_t modsecurity_tx_init(modsec_rec *msr) {
    const char *s = NULL;
    const apr_array_header_t *arr;
    char *semicolon = NULL;
    char *comma = NULL;
    apr_table_entry_t *te;
    int i;

    /* Register TX cleanup */
    apr_pool_cleanup_register(msr->mp, msr, modsecurity_tx_cleanup, apr_pool_cleanup_null);

    /* Initialise C-L */
    msr->request_content_length = -1;
    s = apr_table_get(msr->request_headers, "Content-Length");
    if (s != NULL) {
        msr->request_content_length = strtol(s, NULL, 10);
    }

    /* Figure out whether this request has a body */
    msr->reqbody_chunked = 0;
    msr->reqbody_should_exist = 0;
    if (msr->request_content_length == -1) {
        /* There's no C-L, but is chunked encoding used? */
        char *transfer_encoding = (char *)apr_table_get(msr->request_headers, "Transfer-Encoding");
        if ((transfer_encoding != NULL)&&(m_strcasestr(transfer_encoding, "chunked") != NULL)) {
            msr->reqbody_should_exist = 1;
            msr->reqbody_chunked = 1;
        }
    } else {
        /* C-L found */
        msr->reqbody_should_exist = 1;
    }

    /* Initialise C-T */
    msr->request_content_type = NULL;
    s = apr_table_get(msr->request_headers, "Content-Type");
    if (s != NULL) msr->request_content_type = s;

    /* Decide what to do with the request body. */
    if ((msr->request_content_type != NULL)
       && (strncasecmp(msr->request_content_type, "application/x-www-form-urlencoded", 33) == 0))
    {
        /* Always place POST requests with
         * "application/x-www-form-urlencoded" payloads in memory.
         */
        msr->msc_reqbody_storage = MSC_REQBODY_MEMORY;
        msr->msc_reqbody_spilltodisk = 0;
        msr->msc_reqbody_processor = "URLENCODED";
    } else {
        /* If the C-L is known and there's more data than
         * our limit go to disk straight away.
         */
        if ((msr->request_content_length != -1)
           && (msr->request_content_length > msr->txcfg->reqbody_inmemory_limit))
        {
            msr->msc_reqbody_storage = MSC_REQBODY_DISK;
        }

        /* In all other cases, try using the memory first
         * but switch over to disk for larger bodies.
         */
        msr->msc_reqbody_storage = MSC_REQBODY_MEMORY;
        msr->msc_reqbody_spilltodisk = 1;

        if (msr->request_content_type != NULL) {
            if (strncasecmp(msr->request_content_type, "multipart/form-data", 19) == 0) {
                msr->msc_reqbody_processor = "MULTIPART";
            }
        }
    }

    /* Check if we are forcing buffering, then use memory only. */
    if (msr->txcfg->reqbody_buffering != REQUEST_BODY_FORCEBUF_OFF) {
        msr->msc_reqbody_storage = MSC_REQBODY_MEMORY;
        msr->msc_reqbody_spilltodisk = 0;
    }

    /* Initialise arguments */
    msr->arguments = apr_table_make(msr->mp, 32);
    if (msr->arguments == NULL) return -1;
    if (msr->query_string != NULL) {
        int invalid_count = 0;

        if (parse_arguments(msr, msr->query_string, strlen(msr->query_string),
            msr->txcfg->argument_separator, "QUERY_STRING", msr->arguments,
            &invalid_count) < 0)
        {
            msr_log(msr, 1, "Initialisation: Error occurred while parsing QUERY_STRING arguments.");
            return -1;
        }

        if (invalid_count) {
            msr->urlencoded_error = 1;
        }
    }

    msr->arguments_to_sanitize = apr_table_make(msr->mp, 16);
    if (msr->arguments_to_sanitize == NULL) return -1;
    msr->request_headers_to_sanitize = apr_table_make(msr->mp, 16);
    if (msr->request_headers_to_sanitize == NULL) return -1;
    msr->response_headers_to_sanitize = apr_table_make(msr->mp, 16);
    if (msr->response_headers_to_sanitize == NULL) return -1;
    msr->pattern_to_sanitize = apr_table_make(msr->mp, 32);
    if (msr->pattern_to_sanitize == NULL) return -1;

    /* remove targets */
    msr->removed_targets = apr_table_make(msr->mp, 16);
    if (msr->removed_targets == NULL) return -1;

    /* Initialise cookies */
    msr->request_cookies = apr_table_make(msr->mp, 16);
    if (msr->request_cookies == NULL) return -1;

    /* Initialize matched vars */
    msr->matched_vars = apr_table_make(msr->mp, 8);
    if (msr->matched_vars == NULL) return -1;
    apr_table_clear(msr->matched_vars);

    msr->perf_rules = apr_table_make(msr->mp, 8);
    if (msr->perf_rules == NULL) return -1;
    apr_table_clear(msr->perf_rules);

    /* Locate the cookie headers and parse them */
    arr = apr_table_elts(msr->request_headers);
    te = (apr_table_entry_t *)arr->elts;
    for (i = 0; i < arr->nelts; i++) {
        if (strcasecmp(te[i].key, "Cookie") == 0) {
            if (msr->txcfg->cookie_format == COOKIES_V0) {
                semicolon = apr_pstrdup(msr->mp, te[i].val);
                while((*semicolon != 0)&&(*semicolon != ';')) semicolon++;
                if(*semicolon == ';')    {
                    parse_cookies_v0(msr, te[i].val, msr->request_cookies, ";");
                } else  {
                    comma = apr_pstrdup(msr->mp, te[i].val);
                    while((*comma != 0)&&(*comma != ',')) comma++;
                    if(*comma == ',')    {
                        comma++;
                        if(*comma == 0x20)   {// looks like comma is the separator
                            if (msr->txcfg->debuglog_level >= 5) {
                                msr_log(msr, 5, "Cookie v0 parser: Using comma as a separator. Semi-colon was not identified!");
                            }
                            parse_cookies_v0(msr, te[i].val, msr->request_cookies, ",");
                        } else {
                            parse_cookies_v0(msr, te[i].val, msr->request_cookies, ";");
                        }
                    } else  {
                        parse_cookies_v0(msr, te[i].val, msr->request_cookies, ";");
                    }
                }
            } else {
                parse_cookies_v1(msr, te[i].val, msr->request_cookies);
            }
        }
    }

    /* Collections. */
    msr->tx_vars = apr_table_make(msr->mp, 32);
    if (msr->tx_vars == NULL) return -1;

    msr->geo_vars = apr_table_make(msr->mp, 8);
    if (msr->geo_vars == NULL) return -1;

    msr->collections_original = apr_table_make(msr->mp, 8);
    if (msr->collections_original == NULL) return -1;
    msr->collections = apr_table_make(msr->mp, 8);
    if (msr->collections == NULL) return -1;
    msr->collections_dirty = apr_table_make(msr->mp, 8);
    if (msr->collections_dirty == NULL) return -1;

    /* Other */
    msr->tcache = NULL;
    msr->tcache_items = 0;

    msr->matched_rules = apr_array_make(msr->mp, 16, sizeof(void *));
    if (msr->matched_rules == NULL) return -1;

    msr->matched_var = (msc_string *)apr_pcalloc(msr->mp, sizeof(msc_string));
    if (msr->matched_var == NULL) return -1;

    msr->highest_severity = 255; /* high, invalid value */

    msr->removed_rules = apr_array_make(msr->mp, 16, sizeof(char *));
    if (msr->removed_rules == NULL) return -1;

    msr->removed_rules_tag = apr_array_make(msr->mp, 16, sizeof(char *));
    if (msr->removed_rules_tag == NULL) return -1;

    msr->removed_rules_msg = apr_array_make(msr->mp, 16, sizeof(char *));
    if (msr->removed_rules_msg == NULL) return -1;

    return 1;
}