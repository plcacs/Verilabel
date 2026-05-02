int wc_SignatureGenerateHash(
    enum wc_HashType hash_type, enum wc_SignatureType sig_type,
    const byte* hash_data, word32 hash_len,
    byte* sig, word32 *sig_len,
    const void* key, word32 key_len, WC_RNG* rng)
{
    int ret;

    /* Suppress possible unused arg if all signature types are disabled */
    (void)rng;

    /* Check arguments */
    if (hash_data == NULL || hash_len <= 0 ||
        sig == NULL || sig_len == NULL || *sig_len <= 0 ||
        key == NULL || key_len <= 0) {
        return BAD_FUNC_ARG;
    }

    /* Validate signature len (needs to be at least max) */
    if ((int)*sig_len < wc_SignatureGetSize(sig_type, key, key_len)) {
        WOLFSSL_MSG("wc_SignatureGenerate: Invalid sig type/len");
        return BAD_FUNC_ARG;
    }

    /* Validate hash size */
    ret = wc_HashGetDigestSize(hash_type);
    if (ret < 0) {
        WOLFSSL_MSG("wc_SignatureGenerate: Invalid hash type/len");
        return ret;
    }
    ret = 0;

    /* Create signature using hash as data */
    switch (sig_type) {
        case WC_SIGNATURE_TYPE_ECC:
#if defined(HAVE_ECC) && defined(HAVE_ECC_SIGN)
            /* Create signature using provided ECC key */
            do {
            #ifdef WOLFSSL_ASYNC_CRYPT
                ret = wc_AsyncWait(ret, &((ecc_key*)key)->asyncDev,
                    WC_ASYNC_FLAG_CALL_AGAIN);
            #endif
            if (ret >= 0)
                ret = wc_ecc_sign_hash(hash_data, hash_len, sig, sig_len,
                    rng, (ecc_key*)key);
            } while (ret == WC_PENDING_E);
#else
            ret = SIG_TYPE_E;
#endif
            break;

        case WC_SIGNATURE_TYPE_RSA_W_ENC:
        case WC_SIGNATURE_TYPE_RSA:
#if !defined(NO_RSA) && !defined(WOLFSSL_RSA_PUBLIC_ONLY)
            /* Create signature using provided RSA key */
            do {
            #ifdef WOLFSSL_ASYNC_CRYPT
                ret = wc_AsyncWait(ret, &((RsaKey*)key)->asyncDev,
                    WC_ASYNC_FLAG_CALL_AGAIN);
            #endif
                if (ret >= 0)
                    ret = wc_RsaSSL_Sign(hash_data, hash_len, sig, *sig_len,
                        (RsaKey*)key, rng);
            } while (ret == WC_PENDING_E);
            if (ret >= 0) {
                *sig_len = ret;
                ret = 0; /* Success */
            }
#else
            ret = SIG_TYPE_E;
#endif
            break;

        case WC_SIGNATURE_TYPE_NONE:
        default:
            ret = BAD_FUNC_ARG;
            break;
    }

    return ret;
}