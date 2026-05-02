static int check_passwd(unsigned char *passwd, size_t length)
{
	struct digest *d = NULL;
	unsigned char *passwd1_sum;
	unsigned char *passwd2_sum;
	int ret = 0;
	int hash_len;

	if (IS_ENABLED(CONFIG_PASSWD_CRYPTO_PBKDF2)) {
		hash_len = PBKDF2_LENGTH;
	} else {
		d = digest_alloc(PASSWD_SUM);
		if (!d) {
			pr_err("No such digest: %s\n",
			       PASSWD_SUM ? PASSWD_SUM : "NULL");
			return -ENOENT;
		}

		hash_len = digest_length(d);
	}

	passwd1_sum = calloc(hash_len * 2, sizeof(unsigned char));
	if (!passwd1_sum)
		return -ENOMEM;

	passwd2_sum = passwd1_sum + hash_len;

	if (is_passwd_env_enable())
		ret = read_env_passwd(passwd2_sum, hash_len);
	else if (is_passwd_default_enable())
		ret = read_default_passwd(passwd2_sum, hash_len);
	else
		ret = -EINVAL;

	if (ret < 0)
		goto err;

	if (IS_ENABLED(CONFIG_PASSWD_CRYPTO_PBKDF2)) {
		char *key = passwd2_sum + PBKDF2_SALT_LEN;
		char *salt = passwd2_sum;
		int keylen = PBKDF2_LENGTH - PBKDF2_SALT_LEN;

		ret = pkcs5_pbkdf2_hmac_sha1(passwd, length, salt,
			PBKDF2_SALT_LEN, PBKDF2_COUNT, keylen, passwd1_sum);
		if (ret)
			goto err;

		if (strncmp(passwd1_sum, key, keylen) == 0)
			ret = 1;
	} else {
		ret = digest_digest(d, passwd, length, passwd1_sum);

		if (ret)
			goto err;

		if (strncmp(passwd1_sum, passwd2_sum, hash_len) == 0)
			ret = 1;
	}

err:
	free(passwd1_sum);
	digest_free(d);

	return ret;
}