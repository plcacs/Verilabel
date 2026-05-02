gnutls_x509_crt_import(gnutls_x509_crt_t cert,
		       const gnutls_datum_t * data,
		       gnutls_x509_crt_fmt_t format)
{
	int result = 0;
	int version;

	if (cert == NULL) {
		gnutls_assert();
		return GNUTLS_E_INVALID_REQUEST;
	}

	if (cert->der.data) {
		gnutls_free(cert->der.data);
		cert->der.data = NULL;
	}

	/* If the Certificate is in PEM format then decode it
	 */
	if (format == GNUTLS_X509_FMT_PEM) {
		/* Try the first header */
		result =
		    _gnutls_fbase64_decode(PEM_X509_CERT2, data->data,
					   data->size, &cert->der);

		if (result <= 0) {
			/* try for the second header */
			result =
			    _gnutls_fbase64_decode(PEM_X509_CERT,
						   data->data, data->size,
						   &cert->der);

			if (result < 0) {
				gnutls_assert();
				return result;
			}
		}
	} else {
		result = _gnutls_set_datum(&cert->der, data->data, data->size);
		if (result < 0) {
			gnutls_assert();
			return result;
		}
	}

	if (cert->expanded) {
		/* Any earlier asn1_der_decoding will modify the ASN.1
		   structure, so we need to replace it with a fresh
		   structure. */
		result = crt_reinit(cert);
		if (result < 0) {
			gnutls_assert();
			goto cleanup;
		}
	}

	cert->expanded = 1;

	result =
	    asn1_der_decoding(&cert->cert, cert->der.data, cert->der.size, NULL);
	if (result != ASN1_SUCCESS) {
		result = _gnutls_asn2err(result);
		gnutls_assert();
		goto cleanup;
	}

	result = _gnutls_x509_get_raw_field2(cert->cert, &cert->der,
					  "tbsCertificate.issuer.rdnSequence",
					  &cert->raw_issuer_dn);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	result = _gnutls_x509_get_raw_field2(cert->cert, &cert->der,
					  "tbsCertificate.subject.rdnSequence",
					  &cert->raw_dn);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	result = _gnutls_x509_get_raw_field2(cert->cert, &cert->der,
					  "tbsCertificate.subjectPublicKeyInfo",
					  &cert->raw_spki);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	/* enforce the rule that only version 3 certificates carry extensions */
	version = gnutls_x509_crt_get_version(cert);
	if (version < 3) {
		gnutls_datum_t exts;
		result = _gnutls_x509_get_raw_field2(cert->cert, &cert->der,
			"tbsCertificate.extensions", &exts);
		if (result >= 0 && exts.size > 0) {
			gnutls_assert();
			_gnutls_debug_log("error: extensions present in certificate with version %d\n", version);
			result = GNUTLS_E_X509_CERTIFICATE_ERROR;
			goto cleanup;
		}
	}

	/* Since we do not want to disable any extension
	 */
	cert->use_extensions = 1;

	return 0;

      cleanup:
	_gnutls_free_datum(&cert->der);
	return result;
}