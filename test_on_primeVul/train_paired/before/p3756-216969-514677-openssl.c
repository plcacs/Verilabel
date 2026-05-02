static int ocsp_verify_signer(X509 *signer, int response,
                              X509_STORE *st, unsigned long flags,
                              STACK_OF(X509) *untrusted, STACK_OF(X509) **chain)
{
    X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    X509_VERIFY_PARAM *vp;
    int ret = -1;

    if (ctx == NULL) {
        ERR_raise(ERR_LIB_OCSP, ERR_R_MALLOC_FAILURE);
        goto end;
    }
    if (!X509_STORE_CTX_init(ctx, st, signer, untrusted)) {
        ERR_raise(ERR_LIB_OCSP, ERR_R_X509_LIB);
        goto end;
    }
    if ((vp = X509_STORE_CTX_get0_param(ctx)) == NULL)
        goto end;
    if ((flags & OCSP_PARTIAL_CHAIN) != 0)
        X509_VERIFY_PARAM_set_flags(vp, X509_V_FLAG_PARTIAL_CHAIN);
    if (response
            && X509_get_ext_by_NID(signer, NID_id_pkix_OCSP_noCheck, -1) >= 0)
        /*
         * Locally disable revocation status checking for OCSP responder cert.
         * Done here for CRLs; should be done also for OCSP-based checks.
         */
        X509_VERIFY_PARAM_clear_flags(vp, X509_V_FLAG_CRL_CHECK);
    X509_STORE_CTX_set_purpose(ctx, X509_PURPOSE_OCSP_HELPER);
    X509_STORE_CTX_set_trust(ctx, X509_TRUST_OCSP_REQUEST);

    ret = X509_verify_cert(ctx);
    if (ret <= 0) {
        ret = X509_STORE_CTX_get_error(ctx);
        ERR_raise_data(ERR_LIB_OCSP, OCSP_R_CERTIFICATE_VERIFY_ERROR,
                       "Verify error: %s", X509_verify_cert_error_string(ret));
        goto end;
    }
    if (chain != NULL)
        *chain = X509_STORE_CTX_get1_chain(ctx);

 end:
    X509_STORE_CTX_free(ctx);
    return ret;
}