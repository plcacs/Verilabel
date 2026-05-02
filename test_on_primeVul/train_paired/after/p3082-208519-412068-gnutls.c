int gnutls_x509_ext_import_proxy(const gnutls_datum_t * ext, int *pathlen,
			      char **policyLanguage, char **policy,
			      size_t * sizeof_policy)
{
	ASN1_TYPE c2 = ASN1_TYPE_EMPTY;
	int result;
	gnutls_datum_t value1 = { NULL, 0 };
	gnutls_datum_t value2 = { NULL, 0 };

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
					 &value1);
	if (result < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (policyLanguage) {
		*policyLanguage = (char *)value1.data;
		value1.data = NULL;
	}

	result = _gnutls_x509_read_value(c2, "proxyPolicy.policy", &value2);
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
			*policy = (char *)value2.data;
			value2.data = NULL;
		}
		if (sizeof_policy)
			*sizeof_policy = value2.size;
	}

	result = 0;
 cleanup:
	gnutls_free(value1.data);
	gnutls_free(value2.data);
	asn1_delete_structure(&c2);

	return result;
}