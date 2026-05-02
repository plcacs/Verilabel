copy_ciphersuites(gnutls_session_t session,
		  gnutls_buffer_st * cdata, int add_scsv)
{
	int ret;
	uint8_t cipher_suites[MAX_CIPHERSUITE_SIZE + 2]; /* allow space for SCSV */
	int cipher_suites_size;
	size_t init_length = cdata->length;

	ret =
	    _gnutls_supported_ciphersuites(session, cipher_suites,
					   sizeof(cipher_suites) - 2);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* Here we remove any ciphersuite that does not conform
	 * the certificate requested, or to the
	 * authentication requested (eg SRP).
	 */
	ret =
	    _gnutls_remove_unwanted_ciphersuites(session, cipher_suites,
						 ret, NULL, 0);
	if (ret < 0)
		return gnutls_assert_val(ret);

	/* If no cipher suites were enabled.
	 */
	if (ret == 0)
		return
		    gnutls_assert_val(GNUTLS_E_INSUFFICIENT_CREDENTIALS);

	cipher_suites_size = ret;
	if (add_scsv) {
		cipher_suites[cipher_suites_size] = 0x00;
		cipher_suites[cipher_suites_size + 1] = 0xff;
		cipher_suites_size += 2;

		ret = _gnutls_ext_sr_send_cs(session);
		if (ret < 0)
			return gnutls_assert_val(ret);
	}

	ret =
	    _gnutls_buffer_append_data_prefix(cdata, 16, cipher_suites,
					      cipher_suites_size);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ret = cdata->length - init_length;

	return ret;
}