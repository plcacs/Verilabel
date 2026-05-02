ca_validate_pubkey(struct iked *env, struct iked_static_id *id,
    void *data, size_t len, struct iked_id *out)
{
	BIO		*rawcert = NULL;
	RSA		*peerrsa = NULL, *localrsa = NULL;
	EC_KEY		*peerec = NULL;
	EVP_PKEY	*peerkey = NULL, *localkey = NULL;
	int		 ret = -1;
	FILE		*fp = NULL;
	char		 idstr[IKED_ID_SIZE];
	char		 file[PATH_MAX];
	struct iked_id	 idp;

	switch (id->id_type) {
	case IKEV2_ID_IPV4:
	case IKEV2_ID_FQDN:
	case IKEV2_ID_UFQDN:
	case IKEV2_ID_IPV6:
		break;
	default:
		/* Some types like ASN1_DN will not be mapped to file names */
		log_debug("%s: unsupported public key type %s",
		    __func__, print_map(id->id_type, ikev2_id_map));
		return (-1);
	}

	bzero(&idp, sizeof(idp));
	if ((idp.id_buf = ibuf_new(id->id_data, id->id_length)) == NULL)
		goto done;

	idp.id_type = id->id_type;
	idp.id_offset = id->id_offset;
	if (ikev2_print_id(&idp, idstr, sizeof(idstr)) == -1)
		goto done;

	if (len == 0 && data) {
		/* Data is already an public key */
		peerkey = (EVP_PKEY *)data;
	}
	if (len > 0) {
		if ((rawcert = BIO_new_mem_buf(data, len)) == NULL)
			goto done;

		if ((peerkey = EVP_PKEY_new()) == NULL)
			goto sslerr;
		if ((peerrsa = d2i_RSAPublicKey_bio(rawcert, NULL))) {
			if (!EVP_PKEY_set1_RSA(peerkey, peerrsa)) {
				goto sslerr;
			}
		} else if (BIO_reset(rawcert) == 1 &&
		    (peerec = d2i_EC_PUBKEY_bio(rawcert, NULL))) {
			if (!EVP_PKEY_set1_EC_KEY(peerkey, peerec)) {
				goto sslerr;
			}
		} else {
			log_debug("%s: unknown key type received", __func__);
			goto sslerr;
		}
	}

	lc_idtype(idstr);
	if (strlcpy(file, IKED_PUBKEY_DIR, sizeof(file)) >= sizeof(file) ||
	    strlcat(file, idstr, sizeof(file)) >= sizeof(file)) {
		log_debug("%s: public key id too long %s", __func__, idstr);
		goto done;
	}

	if ((fp = fopen(file, "r")) == NULL) {
		/* Log to debug when called from ca_validate_cert */
		logit(len == 0 ? LOG_DEBUG : LOG_INFO,
		    "%s: could not open public key %s", __func__, file);
		goto done;
	}
	localkey = PEM_read_PUBKEY(fp, NULL, NULL, NULL);
	if (localkey == NULL) {
		/* reading PKCS #8 failed, try PEM RSA */
		rewind(fp);
		localrsa = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL);
		fclose(fp);
		if (localrsa == NULL)
			goto sslerr;
		if ((localkey = EVP_PKEY_new()) == NULL)
			goto sslerr;
		if (!EVP_PKEY_set1_RSA(localkey, localrsa))
			goto sslerr;
	} else {
		fclose(fp);
	}
	if (localkey == NULL)
		goto sslerr;

	if (peerkey && !EVP_PKEY_cmp(peerkey, localkey)) {
		log_debug("%s: public key does not match %s", __func__, file);
		goto done;
	}

	log_debug("%s: valid public key in file %s", __func__, file);

	if (out && ca_pubkey_serialize(localkey, out))
		goto done;

	ret = 0;
 sslerr:
	if (ret != 0)
		ca_sslerror(__func__);
 done:
	ibuf_release(idp.id_buf);
	if (localkey != NULL)
		EVP_PKEY_free(localkey);
	if (peerrsa != NULL)
		RSA_free(peerrsa);
	if (peerec != NULL)
		EC_KEY_free(peerec);
	if (localrsa != NULL)
		RSA_free(localrsa);
	if (rawcert != NULL) {
		BIO_free(rawcert);
		if (peerkey != NULL)
			EVP_PKEY_free(peerkey);
	}

	return (ret);
}