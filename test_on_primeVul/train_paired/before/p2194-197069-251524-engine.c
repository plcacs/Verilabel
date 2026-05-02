static int pkey_gost2018_encrypt(EVP_PKEY_CTX *pctx, unsigned char *out,
                          size_t *out_len, const unsigned char *key,
                          size_t key_len)
{
    PSKeyTransport_gost *pst = NULL;
    EVP_PKEY *pubk = EVP_PKEY_CTX_get0_pkey(pctx);
    struct gost_pmeth_data *data = EVP_PKEY_CTX_get_data(pctx);
    int pkey_nid = EVP_PKEY_base_id(pubk);
    unsigned char expkeys[64];
    EVP_PKEY *sec_key = NULL;
    int ret = 0;
    int mac_nid = NID_undef;
    size_t mac_len = 0;
    int exp_len = 0, iv_len = 0;
    unsigned char *exp_buf = NULL;
    int key_is_ephemeral = 0;

    switch (data->cipher_nid) {
    case NID_magma_ctr:
        mac_nid = NID_magma_mac;
        mac_len = 8;
        iv_len = 4;
        break;
    case NID_grasshopper_ctr:
        mac_nid = NID_grasshopper_mac;
        mac_len = 16;
        iv_len = 8;
        break;
    default:
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, GOST_R_INVALID_CIPHER);
        return -1;
        break;
    }
    exp_len = key_len + mac_len;
    exp_buf = OPENSSL_malloc(exp_len);
    if (!exp_buf) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, ERR_R_MALLOC_FAILURE);
        return -1;
    }

    sec_key = EVP_PKEY_CTX_get0_peerkey(pctx);
    if (!sec_key)
    {
      sec_key = EVP_PKEY_new();
      if (sec_key == NULL) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, ERR_R_MALLOC_FAILURE );
        goto err;
      }

      if (!EVP_PKEY_assign(sec_key, EVP_PKEY_base_id(pubk), EC_KEY_new())
          || !EVP_PKEY_copy_parameters(sec_key, pubk)
          || !gost_ec_keygen(EVP_PKEY_get0(sec_key))) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT,
            GOST_R_ERROR_COMPUTING_SHARED_KEY);
        goto err;
      }
      key_is_ephemeral = 1;
    }

    if (data->shared_ukm_size == 0) {
        if (RAND_bytes(data->shared_ukm, 32) <= 0) {
            GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, ERR_R_INTERNAL_ERROR);
            goto err;
        }
        data->shared_ukm_size = 32;
    }

    if (gost_keg(data->shared_ukm, pkey_nid,
                 EC_KEY_get0_public_key(EVP_PKEY_get0(pubk)),
                 EVP_PKEY_get0(sec_key), expkeys) <= 0) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT,
                GOST_R_ERROR_COMPUTING_EXPORT_KEYS);
        goto err;
    }

    if (gost_kexp15(key, key_len, data->cipher_nid, expkeys + 32,
                    mac_nid, expkeys + 0, data->shared_ukm + 24, iv_len,
                    exp_buf, &exp_len) <= 0) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, GOST_R_CANNOT_PACK_EPHEMERAL_KEY);
        goto err;
    }

    pst = PSKeyTransport_gost_new();
    if (!pst) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    pst->ukm = ASN1_OCTET_STRING_new();
    if (pst->ukm == NULL) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!ASN1_OCTET_STRING_set(pst->ukm, data->shared_ukm, data->shared_ukm_size)) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!ASN1_OCTET_STRING_set(pst->psexp, exp_buf, exp_len)) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (!X509_PUBKEY_set(&pst->ephem_key, out ? sec_key : pubk)) {
        GOSTerr(GOST_F_PKEY_GOST2018_ENCRYPT, GOST_R_CANNOT_PACK_EPHEMERAL_KEY);
        goto err;
    }

    if ((*out_len = i2d_PSKeyTransport_gost(pst, out ? &out : NULL)) > 0)
        ret = 1;
 err:
    OPENSSL_cleanse(expkeys, sizeof(expkeys));
    if (key_is_ephemeral)
        EVP_PKEY_free(sec_key);

    PSKeyTransport_gost_free(pst);
    OPENSSL_free(exp_buf);
    return ret;
}