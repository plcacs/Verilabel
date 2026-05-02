static int cbtls_verify(int ok, X509_STORE_CTX *ctx)
{
	char subject[1024]; /* Used for the subject name */
	char issuer[1024]; /* Used for the issuer name */
	char common_name[1024];
	char cn_str[1024];
	char buf[64];
	EAP_HANDLER *handler = NULL;
	X509 *client_cert;
	X509 *issuer_cert;
	SSL *ssl;
	int err, depth, lookup, loc;
	EAP_TLS_CONF *conf;
	int my_ok = ok;
	REQUEST *request;
	ASN1_INTEGER *sn = NULL;
	ASN1_TIME *asn_time = NULL;
#ifdef HAVE_OPENSSL_OCSP_H
	X509_STORE *ocsp_store = NULL;
#endif

	client_cert = X509_STORE_CTX_get_current_cert(ctx);
	err = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);

	lookup = depth;

	/*
	 *	Log client/issuing cert.  If there's an error, log
	 *	issuing cert.
	 */
	if ((lookup > 1) && !my_ok) lookup = 1;

	/*
	 * Retrieve the pointer to the SSL of the connection currently treated
	 * and the application specific data stored into the SSL object.
	 */
	ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	handler = (EAP_HANDLER *)SSL_get_ex_data(ssl, 0);
	request = handler->request;
	conf = (EAP_TLS_CONF *)SSL_get_ex_data(ssl, 1);
#ifdef HAVE_OPENSSL_OCSP_H
	ocsp_store = (X509_STORE *)SSL_get_ex_data(ssl, 2);
#endif


	/*
	 *	Get the Serial Number
	 */
	buf[0] = '\0';
	sn = X509_get_serialNumber(client_cert);

	/*
	 *	For this next bit, we create the attributes *only* if
	 *	we're at the client or issuing certificate.
	 */
	if ((lookup <= 1) && sn && (sn->length < (sizeof(buf) / 2))) {
		char *p = buf;
		int i;

		for (i = 0; i < sn->length; i++) {
			sprintf(p, "%02x", (unsigned int)sn->data[i]);
			p += 2;
		}
		pairadd(&handler->certs,
			pairmake(cert_attr_names[EAPTLS_SERIAL][lookup], buf, T_OP_SET));
	}


	/*
	 *	Get the Expiration Date
	 */
	buf[0] = '\0';
	asn_time = X509_get_notAfter(client_cert);
	if ((lookup <= 1) && asn_time && (asn_time->length < MAX_STRING_LEN)) {
		memcpy(buf, (char*) asn_time->data, asn_time->length);
		buf[asn_time->length] = '\0';
		pairadd(&handler->certs,
			pairmake(cert_attr_names[EAPTLS_EXPIRATION][lookup], buf, T_OP_SET));
	}

	/*
	 *	Get the Subject & Issuer
	 */
	subject[0] = issuer[0] = '\0';
	X509_NAME_oneline(X509_get_subject_name(client_cert), subject,
			  sizeof(subject));
	subject[sizeof(subject) - 1] = '\0';
	if ((lookup <= 1) && subject[0] && (strlen(subject) < MAX_STRING_LEN)) {
		pairadd(&handler->certs,
			pairmake(cert_attr_names[EAPTLS_SUBJECT][lookup], subject, T_OP_SET));
	}

	X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), issuer,
			  sizeof(issuer));
	issuer[sizeof(issuer) - 1] = '\0';
	if ((lookup <= 1) && issuer[0] && (strlen(issuer) < MAX_STRING_LEN)) {
		pairadd(&handler->certs,
			pairmake(cert_attr_names[EAPTLS_ISSUER][lookup], issuer, T_OP_SET));
	}

	/*
	 *	Get the Common Name, if there is a subject.
	 */
	X509_NAME_get_text_by_NID(X509_get_subject_name(client_cert),
				  NID_commonName, common_name, sizeof(common_name));
	common_name[sizeof(common_name) - 1] = '\0';
	if ((lookup <= 1) && common_name[0] && subject[0] && (strlen(common_name) < MAX_STRING_LEN)) {
		pairadd(&handler->certs,
			pairmake(cert_attr_names[EAPTLS_CN][lookup], common_name, T_OP_SET));
	}

#ifdef GEN_EMAIL
	/*
	 *	Get the RFC822 Subject Alternative Name
	 */
	loc = X509_get_ext_by_NID(client_cert, NID_subject_alt_name, 0);
	if (lookup <= 1 && loc >= 0) {
		X509_EXTENSION *ext = NULL;
		GENERAL_NAMES *names = NULL;
		int i;

		if ((ext = X509_get_ext(client_cert, loc)) &&
		    (names = X509V3_EXT_d2i(ext))) {
			for (i = 0; i < sk_GENERAL_NAME_num(names); i++) {
				GENERAL_NAME *name = sk_GENERAL_NAME_value(names, i);

				switch (name->type) {
				case GEN_EMAIL:
					if (ASN1_STRING_length(name->d.rfc822Name) >= MAX_STRING_LEN)
						break;

					pairadd(&handler->certs,
						pairmake(cert_attr_names[EAPTLS_SAN_EMAIL][lookup],
							 ASN1_STRING_data(name->d.rfc822Name), T_OP_SET));
					break;
				default:
					/* XXX TODO handle other SAN types */
					break;
				}
			}
		}
		if (names != NULL)
			sk_GENERAL_NAME_free(names);
	}
#endif	/* GEN_EMAIL */

	/*
	 *	If the CRL has expired, that might still be OK.
	 */
	if (!my_ok &&
	    (conf->allow_expired_crl) &&
	    (err == X509_V_ERR_CRL_HAS_EXPIRED)) {
		my_ok = 1;
		X509_STORE_CTX_set_error( ctx, 0 );
	}

	if (!my_ok) {
		const char *p = X509_verify_cert_error_string(err);
		radlog(L_ERR,"--> verify error:num=%d:%s\n",err, p);
		radius_pairmake(request, &request->packet->vps,
				"Module-Failure-Message", p, T_OP_SET);
		return my_ok;
	}

	switch (ctx->error) {

	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		radlog(L_ERR, "issuer= %s\n", issuer);
		break;
	case X509_V_ERR_CERT_NOT_YET_VALID:
	case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
		radlog(L_ERR, "notBefore=");
#if 0
		ASN1_TIME_print(bio_err, X509_get_notBefore(ctx->current_cert));
#endif
		break;
	case X509_V_ERR_CERT_HAS_EXPIRED:
	case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
		radlog(L_ERR, "notAfter=");
#if 0
		ASN1_TIME_print(bio_err, X509_get_notAfter(ctx->current_cert));
#endif
		break;
	}

	/*
	 *	If we're at the actual client cert, apply additional
	 *	checks.
	 */
	if (depth == 0) {
		/*
		 *	If the conf tells us to, check cert issuer
		 *	against the specified value and fail
		 *	verification if they don't match.
		 */
		if (conf->check_cert_issuer &&
		    (strcmp(issuer, conf->check_cert_issuer) != 0)) {
			radlog(L_AUTH, "rlm_eap_tls: Certificate issuer (%s) does not match specified value (%s)!", issuer, conf->check_cert_issuer);
 			my_ok = 0;
 		}

		/*
		 *	If the conf tells us to, check the CN in the
		 *	cert against xlat'ed value, but only if the
		 *	previous checks passed.
		 */
		if (my_ok && conf->check_cert_cn) {
			if (!radius_xlat(cn_str, sizeof(cn_str), conf->check_cert_cn, handler->request, NULL)) {
				radlog(L_ERR, "rlm_eap_tls (%s): xlat failed.",
				       conf->check_cert_cn);
				/* if this fails, fail the verification */
				my_ok = 0;
			} else {
				RDEBUG2("checking certificate CN (%s) with xlat'ed value (%s)", common_name, cn_str);
				if (strcmp(cn_str, common_name) != 0) {
					radlog(L_AUTH, "rlm_eap_tls: Certificate CN (%s) does not match specified value (%s)!", common_name, cn_str);
					my_ok = 0;
				}
			}
		} /* check_cert_cn */

#ifdef HAVE_OPENSSL_OCSP_H
		if (my_ok && conf->ocsp_enable){
			RDEBUG2("--> Starting OCSP Request");
			if(X509_STORE_CTX_get1_issuer(&issuer_cert, ctx, client_cert)!=1) {
				radlog(L_ERR, "Error: Couldn't get issuer_cert for %s", common_name);
			}
			my_ok = ocsp_check(ocsp_store, issuer_cert, client_cert, conf);
		}
#endif

		while (conf->verify_client_cert_cmd) {
			char filename[256];
			int fd;
			FILE *fp;

			snprintf(filename, sizeof(filename), "%s/%s.client.XXXXXXXX",
				 conf->verify_tmp_dir, progname);
			fd = mkstemp(filename);
			if (fd < 0) {
				RDEBUG("Failed creating file in %s: %s",
				       conf->verify_tmp_dir, strerror(errno));
				break;				       
			}

			fp = fdopen(fd, "w");
			if (!fp) {
				RDEBUG("Failed opening file %s: %s",
				       filename, strerror(errno));
				break;
			}

			if (!PEM_write_X509(fp, client_cert)) {
				fclose(fp);
				RDEBUG("Failed writing certificate to file");
				goto do_unlink;
			}
			fclose(fp);

			if (!radius_pairmake(request, &request->packet->vps,
					     "TLS-Client-Cert-Filename",
					     filename, T_OP_SET)) {
				RDEBUG("Failed creating TLS-Client-Cert-Filename");
				
				goto do_unlink;
			}

			RDEBUG("Verifying client certificate: %s",
			       conf->verify_client_cert_cmd);
			if (radius_exec_program(conf->verify_client_cert_cmd,
						request, 1, NULL, 0, 
						request->packet->vps,
						NULL, 1) != 0) {
				radlog(L_AUTH, "rlm_eap_tls: Certificate CN (%s) fails external verification!", common_name);
				my_ok = 0;
			} else {
				RDEBUG("Client certificate CN %s passed external validation", common_name);
			}

		do_unlink:
			unlink(filename);
			break;
		}


	} /* depth == 0 */

	if (debug_flag > 0) {
		RDEBUG2("chain-depth=%d, ", depth);
		RDEBUG2("error=%d", err);

		RDEBUG2("--> User-Name = %s", handler->identity);
		RDEBUG2("--> BUF-Name = %s", common_name);
		RDEBUG2("--> subject = %s", subject);
		RDEBUG2("--> issuer  = %s", issuer);
		RDEBUG2("--> verify return:%d", my_ok);
	}
	return my_ok;
}