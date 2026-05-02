gnutls_ocsp_resp_check_crt(gnutls_ocsp_resp_t resp,
			   unsigned int indx, gnutls_x509_crt_t crt)
{
	int ret;
	gnutls_digest_algorithm_t digest;
	gnutls_datum_t rdn_hash = { NULL, 0 }, rserial = {
	NULL, 0};
	gnutls_datum_t cserial = { NULL, 0 };
	gnutls_datum_t dn = { NULL, 0 };
	uint8_t cdn_hash[MAX_HASH_SIZE];
	size_t t, hash_len;

	ret =
	    gnutls_ocsp_resp_get_single(resp, indx, &digest, &rdn_hash,
					NULL, &rserial, NULL, NULL, NULL,
					NULL, NULL);
	if (ret < 0)
		return gnutls_assert_val(ret);

	if (rserial.size == 0 || digest == GNUTLS_DIG_UNKNOWN) {
		ret = gnutls_assert_val(GNUTLS_E_OCSP_RESPONSE_ERROR);
		goto cleanup;
	}

	hash_len = _gnutls_hash_get_algo_len(hash_to_entry(digest));
	if (hash_len != rdn_hash.size) {
		ret = gnutls_assert_val(GNUTLS_E_OCSP_RESPONSE_ERROR);
		goto cleanup;
	}

	cserial.size = rserial.size;
	cserial.data = gnutls_malloc(cserial.size);
	if (cserial.data == NULL) {
		ret = gnutls_assert_val(GNUTLS_E_MEMORY_ERROR);
		goto cleanup;
	}

	t = cserial.size;
	ret = gnutls_x509_crt_get_serial(crt, cserial.data, &t);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (rserial.size != cserial.size
	    || memcmp(cserial.data, rserial.data, rserial.size) != 0) {
		ret = GNUTLS_E_OCSP_RESPONSE_ERROR;
		gnutls_assert();
		goto cleanup;
	}

	ret = gnutls_x509_crt_get_raw_issuer_dn(crt, &dn);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	ret = _gnutls_hash_fast(digest, dn.data, dn.size, cdn_hash);
	if (ret < 0) {
		gnutls_assert();
		goto cleanup;
	}

	if (memcmp(cdn_hash, rdn_hash.data, hash_len) != 0) {
		ret = GNUTLS_E_OCSP_RESPONSE_ERROR;
		gnutls_assert();
		goto cleanup;
	}

	ret = 0;

      cleanup:
	gnutls_free(rdn_hash.data);
	gnutls_free(rserial.data);
	gnutls_free(cserial.data);
	gnutls_free(dn.data);

	return ret;
}