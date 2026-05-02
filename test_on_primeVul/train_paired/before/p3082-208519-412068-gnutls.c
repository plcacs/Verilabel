int gnutls_x509_ext_import_proxy(const gnutls_datum_t * ext, int *pathlen,
			      char **policyLanguage, char **policy,
			      size_t * sizeof_policy)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result;
	gnutls_datum_t value = { NULL, 0 };

	if ((result = asn1_create_element
	     (_gnutls_get_pkix(), "PKIX1.ProxyCertInfo",
	      &c2)) != ASN1_SUCCESS) {
		gnutls_assert();
		return _gnutls_asn2err(result);
	}

	result = _asn1_strict_der_decode(&c2, ext->data, ext->size, NULL);
	if (result != ASN1_SUCCESS) {
		gnutls_assert();
		result = _gnutls_asn2err(result);
		goto cleanup;
	}

	if (pathlen) {
		result = _gnutls_x509_read_uint(c2, "pCPathLenConstraint",
						(unsigned int *)
						pathlen);
		if (result == GNUTLS_E_ASN1_ELEMENT_NOT_FOUND)
			*pathlen = -1;
		else if (result != GNUTLS_E_SUCCESS) {
			gnutls_assert();
			result = _gnutls_asn2err(result);
			goto cleanup;
		}
	}

	result = _gnutls_x509_read_value(c2, "proxyPolicy.policyLanguage",
					 &value);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (policyLanguage) {
		*policyLanguage = (char *)value.data;
	} else {
		gnutls_free(value.data);
		value.data = NULL;
	}

	result = _gnutls_x509_read_value(c2, "proxyPolicy.policy", &value);
	if (result == GNUTLS_E_ASN1_ELEMENT_NOT_FOUND) {
		if (policy)
			*policy = NULL;
		if (sizeof_policy)
			*sizeof_policy = 0;
	} else if (result < 0) {
		gnutls_assert();
		goto cleanup;
	} else {
		if (policy) {
			*policy = (char *)value.data;
			value.data = NULL;
		}
		if (sizeof_policy)
			*sizeof_policy = value.size;
	}

	result = 0;
 cleanup:
	gnutls_free(value.data);
	asn1_delete_structure(&c2);

	return result;
}