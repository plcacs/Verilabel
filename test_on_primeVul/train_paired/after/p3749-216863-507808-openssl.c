static int append_ia5(STACK_OF(OPENSSL_STRING) **sk, const ASN1_IA5STRING *email)
{
    char *emtmp;
    /* First some sanity checks */
    if (email->type != V_ASN1_IA5STRING)
        return 1;
    if (email->data == NULL || email->length == 0)
        return 1;
    if (memchr(email->data, 0, email->length) != NULL)
        return 1;
    if (*sk == NULL)
        *sk = sk_OPENSSL_STRING_new(sk_strcmp);
    if (*sk == NULL)
        return 0;

    emtmp = OPENSSL_strndup((char *)email->data, email->length);
    if (emtmp == NULL)
        return 0;

    /* Don't add duplicates */
    if (sk_OPENSSL_STRING_find(*sk, emtmp) != -1) {
        OPENSSL_free(emtmp);
        return 1;
    }
    if (!sk_OPENSSL_STRING_push(*sk, emtmp)) {
        OPENSSL_free(emtmp); /* free on push failure */
        X509_email_free(*sk);
        *sk = NULL;
        return 0;
    }
    return 1;
}