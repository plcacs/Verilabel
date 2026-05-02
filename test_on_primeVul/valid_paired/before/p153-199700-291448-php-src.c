static int php_openssl_validate_iv(char **piv, size_t *piv_len, size_t iv_required_len,
		zend_bool *free_iv, EVP_CIPHER_CTX *cipher_ctx, struct php_openssl_cipher_mode *mode) /* {{{ */
{
	char *iv_new;

	/* Best case scenario, user behaved */
	if (*piv_len == iv_required_len) {
		return SUCCESS;
	}

	if (mode->is_aead) {
		if (EVP_CIPHER_CTX_ctrl(cipher_ctx, mode->aead_ivlen_flag, *piv_len, NULL) != 1) {
			php_error_docref(NULL, E_WARNING, "Setting of IV length for AEAD mode failed");
			return FAILURE;
		}
		return SUCCESS;
	}

	iv_new = ecalloc(1, iv_required_len + 1);

	if (*piv_len == 0) {
		/* BC behavior */
		*piv_len = iv_required_len;
		*piv = iv_new;
		*free_iv = 1;
		return SUCCESS;

	}

	if (*piv_len < iv_required_len) {
		php_error_docref(NULL, E_WARNING,
				"IV passed is only %zd bytes long, cipher expects an IV of precisely %zd bytes, padding with \\0",
				*piv_len, iv_required_len);
		memcpy(iv_new, *piv, *piv_len);
		*piv_len = iv_required_len;
		*piv = iv_new;
		*free_iv = 1;
		return SUCCESS;
	}

	php_error_docref(NULL, E_WARNING,
			"IV passed is %zd bytes long which is longer than the %zd expected by selected cipher, truncating",
			*piv_len, iv_required_len);
	memcpy(iv_new, *piv, iv_required_len);
	*piv_len = iv_required_len;
	*piv = iv_new;
	*free_iv = 1;
	return SUCCESS;

}