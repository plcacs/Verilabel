static apr_byte_t oidc_validate_post_logout_url(request_rec *r, const char *url,
		char **err_str, char **err_desc) {
	apr_uri_t uri;
	const char *c_host = NULL;

	if (apr_uri_parse(r->pool, url, &uri) != APR_SUCCESS) {
		*err_str = apr_pstrdup(r->pool, "Malformed URL");
		*err_desc = apr_psprintf(r->pool, "Logout URL malformed: %s", url);
		oidc_error(r, "%s: %s", *err_str, *err_desc);
		return FALSE;
	}

	c_host = oidc_get_current_url_host(r);
	if ((uri.hostname != NULL)
			&& ((strstr(c_host, uri.hostname) == NULL)
					|| (strstr(uri.hostname, c_host) == NULL))) {
		*err_str = apr_pstrdup(r->pool, "Invalid Request");
		*err_desc =
				apr_psprintf(r->pool,
						"logout value \"%s\" does not match the hostname of the current request \"%s\"",
						apr_uri_unparse(r->pool, &uri, 0), c_host);
		oidc_error(r, "%s: %s", *err_str, *err_desc);
		return FALSE;
	} else if (strstr(url, "/") != url) {
		*err_str = apr_pstrdup(r->pool, "Malformed URL");
		*err_desc =
				apr_psprintf(r->pool,
						"No hostname was parsed and it does not seem to be relative, i.e starting with '/': %s",
						url);
		oidc_error(r, "%s: %s", *err_str, *err_desc);
		return FALSE;
	}

	/* validate the URL to prevent HTTP header splitting */
	if (((strstr(url, "\n") != NULL) || strstr(url, "\r") != NULL)) {
		*err_str = apr_pstrdup(r->pool, "Invalid Request");
		*err_desc =
				apr_psprintf(r->pool,
						"logout value \"%s\" contains illegal \"\n\" or \"\r\" character(s)",
						url);
		oidc_error(r, "%s: %s", *err_str, *err_desc);
		return FALSE;
	}

	return TRUE;
}