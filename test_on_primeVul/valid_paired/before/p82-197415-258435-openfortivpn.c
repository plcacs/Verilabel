static int ssl_verify_cert(struct tunnel *tunnel)
{
	int ret = -1;
	int cert_valid = 0;
	unsigned char digest[SHA256LEN];
	unsigned int len;
	struct x509_digest *elem;
	char digest_str[SHA256STRLEN], *subject, *issuer;
	char *line;
	int i;
	X509_NAME *subj;

	SSL_set_verify(tunnel->ssl_handle, SSL_VERIFY_PEER, NULL);

	X509 *cert = SSL_get_peer_certificate(tunnel->ssl_handle);
	if (cert == NULL) {
		log_error("Unable to get gateway certificate.\n");
		return 1;
	}

	subj = X509_get_subject_name(cert);

#ifdef HAVE_X509_CHECK_HOST
	// Use OpenSSL native host validation if v >= 1.0.2.
	// compare against gateway_host and correctly check return value
	// to fix piror Incorrect use of X509_check_host
	if (X509_check_host(cert, tunnel->config->gateway_host,
	                    0, 0, NULL) == 1)
		cert_valid = 1;
#else
	char common_name[FIELD_SIZE + 1];
	// Use explicit Common Name check if native validation not available.
	// Note: this will ignore Subject Alternative Name fields.
	if (subj
	    && X509_NAME_get_text_by_NID(subj, NID_commonName, common_name,
	                                 FIELD_SIZE) > 0
	    && strncasecmp(common_name, tunnel->config->gateway_host,
	                   FIELD_SIZE) == 0)
		cert_valid = 1;
#endif

	// Try to validate certificate using local PKI
	if (cert_valid
	    && SSL_get_verify_result(tunnel->ssl_handle) == X509_V_OK) {
		log_debug("Gateway certificate validation succeeded.\n");
		ret = 0;
		goto free_cert;
	}
	log_debug("Gateway certificate validation failed.\n");

	// If validation failed, check if cert is in the white list
	if (X509_digest(cert, EVP_sha256(), digest, &len) <= 0
	    || len != SHA256LEN) {
		log_error("Could not compute certificate sha256 digest.\n");
		goto free_cert;
	}
	// Encode digest in base16
	for (i = 0; i < SHA256LEN; i++)
		sprintf(&digest_str[2 * i], "%02x", digest[i]);
	digest_str[SHA256STRLEN - 1] = '\0';
	// Is it in whitelist?
	for (elem = tunnel->config->cert_whitelist; elem != NULL;
	     elem = elem->next)
		if (memcmp(digest_str, elem->data, SHA256STRLEN - 1) == 0)
			break;
	if (elem != NULL) { // break before end of loop
		log_debug("Gateway certificate digest found in white list.\n");
		ret = 0;
		goto free_cert;
	}

	subject = X509_NAME_oneline(subj, NULL, 0);
	issuer = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);

	log_error("Gateway certificate validation failed, and the certificate digest in not in the local whitelist. If you trust it, rerun with:\n");
	log_error("    --trusted-cert %s\n", digest_str);
	log_error("or add this line to your config file:\n");
	log_error("    trusted-cert = %s\n", digest_str);
	log_error("Gateway certificate:\n");
	log_error("    subject:\n");
	for (line = strtok(subject, "/"); line != NULL;
	     line = strtok(NULL, "/"))
		log_error("        %s\n", line);
	log_error("    issuer:\n");
	for (line = strtok(issuer, "/"); line != NULL;
	     line = strtok(NULL, "/"))
		log_error("        %s\n", line);
	log_error("    sha256 digest:\n");
	log_error("        %s\n", digest_str);

free_cert:
	X509_free(cert);
	return ret;
}