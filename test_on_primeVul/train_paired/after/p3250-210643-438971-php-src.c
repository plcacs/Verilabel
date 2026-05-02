   Send an email message */
PHP_FUNCTION(imap_mail)
{
	char *to=NULL, *message=NULL, *headers=NULL, *subject=NULL, *cc=NULL, *bcc=NULL, *rpath=NULL;
	int to_len, message_len, headers_len, subject_len, cc_len, bcc_len, rpath_len, argc = ZEND_NUM_ARGS();

	if (zend_parse_parameters(argc TSRMLS_CC, "sss|ssss", &to, &to_len, &subject, &subject_len, &message, &message_len,
		&headers, &headers_len, &cc, &cc_len, &bcc, &bcc_len, &rpath, &rpath_len) == FAILURE) {
		return;
	}

	/* To: */
	if (!to_len) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "No to field in mail command");
		RETURN_FALSE;
	}

	/* Subject: */
	if (!subject_len) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "No subject field in mail command");
		RETURN_FALSE;
	}

	/* message body */
	if (!message_len) {
		/* this is not really an error, so it is allowed. */
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "No message string in mail command");
	}

	if (_php_imap_mail(to, subject, message, headers, cc, bcc, rpath TSRMLS_CC)) {
		RETURN_TRUE;
	} else {
		RETURN_FALSE;
	}