void php_filter_validate_url(PHP_INPUT_FILTER_PARAM_DECL) /* {{{ */
{
	php_url *url;
	size_t old_len = Z_STRLEN_P(value);

	if (flags & (FILTER_FLAG_SCHEME_REQUIRED | FILTER_FLAG_HOST_REQUIRED)) {
		php_error_docref(NULL, E_DEPRECATED,
			"explicit use of FILTER_FLAG_SCHEME_REQUIRED and FILTER_FLAG_HOST_REQUIRED is deprecated");
	}

	php_filter_url(value, flags, option_array, charset);

	if (Z_TYPE_P(value) != IS_STRING || old_len != Z_STRLEN_P(value)) {
		RETURN_VALIDATION_FAILED
	}

	/* Use parse_url - if it returns false, we return NULL */
	url = php_url_parse_ex(Z_STRVAL_P(value), Z_STRLEN_P(value));

	if (url == NULL) {
		RETURN_VALIDATION_FAILED
	}

	if (url->scheme != NULL &&
		(zend_string_equals_literal_ci(url->scheme, "http") || zend_string_equals_literal_ci(url->scheme, "https"))) {
		char *e, *s, *t;
		size_t l;

		if (url->host == NULL) {
			goto bad_url;
		}

		s = ZSTR_VAL(url->host);
		l = ZSTR_LEN(url->host);
		e = s + l;
		t = e - 1;

		/* An IPv6 enclosed by square brackets is a valid hostname */
		if (*s == '[' && *t == ']' && _php_filter_validate_ipv6((s + 1), l - 2)) {
			php_url_free(url);
			return;
		}

		// Validate domain
		if (!_php_filter_validate_domain(ZSTR_VAL(url->host), l, FILTER_FLAG_HOSTNAME)) {
			php_url_free(url);
			RETURN_VALIDATION_FAILED
		}
	}

	if (
		url->scheme == NULL ||
		/* some schemas allow the host to be empty */
		(url->host == NULL && (strcmp(ZSTR_VAL(url->scheme), "mailto") && strcmp(ZSTR_VAL(url->scheme), "news") && strcmp(ZSTR_VAL(url->scheme), "file"))) ||
		((flags & FILTER_FLAG_PATH_REQUIRED) && url->path == NULL) || ((flags & FILTER_FLAG_QUERY_REQUIRED) && url->query == NULL)
	) {
bad_url:
		php_url_free(url);
		RETURN_VALIDATION_FAILED
	}

	if (url->user != NULL && !is_userinfo_valid(url->user)) {
		php_url_free(url);
		RETURN_VALIDATION_FAILED

	}

	php_url_free(url);
}