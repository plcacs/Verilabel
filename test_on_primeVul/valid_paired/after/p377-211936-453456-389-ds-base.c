crypt_pw_cmp(const char *userpwd, const char *dbpwd)
{
    int rc = -1;
    char *cp = NULL;
    size_t dbpwd_len = strlen(dbpwd);
    struct crypt_data data;
    data.initialized = 0;

    /*
     * there MUST be at least 2 chars of salt and some pw bytes, else this is INVALID and will
     * allow any password to bind as we then only compare SALTS.
     */
    if (dbpwd_len >= 3) {
        /* we use salt (first 2 chars) of encoded password in call to crypt_r() */
        cp = crypt_r(userpwd, dbpwd, &data);
    }
    /* If these are not the same length, we can not proceed safely with memcmp. */
    if (cp && dbpwd_len == strlen(cp)) {
        rc = slapi_ct_memcmp(dbpwd, cp, dbpwd_len);
    } else {
        rc = -1;
    }
    return rc;
}