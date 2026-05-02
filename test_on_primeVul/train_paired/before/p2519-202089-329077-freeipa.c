krb5_error_code ipadb_get_pwd_policy(krb5_context kcontext, char *name,
                                     osa_policy_ent_t *policy)
{
    struct ipadb_context *ipactx;
    char *esc_name = NULL;
    char *src_filter = NULL;
    krb5_error_code kerr;
    LDAPMessage *res = NULL;
    LDAPMessage *lentry;
    osa_policy_ent_t pentry = NULL;
    uint32_t result;
    int ret;

    ipactx = ipadb_get_context(kcontext);
    if (!ipactx) {
        return KRB5_KDB_DBNOTINITED;
    }

    esc_name = ipadb_filter_escape(name, true);
    if (!esc_name) {
        return ENOMEM;
    }

    ret = asprintf(&src_filter, POLICY_SEARCH_FILTER, esc_name);
    if (ret == -1) {
        kerr = KRB5_KDB_INTERNAL_ERROR;
        goto done;
    }

    kerr = ipadb_simple_search(ipactx,
                               ipactx->realm_base, LDAP_SCOPE_SUBTREE,
                               src_filter, std_pwdpolicy_attrs, &res);
    if (kerr) {
        goto done;
    }

    lentry = ldap_first_entry(ipactx->lcontext, res);
    if (!lentry) {
        kerr = KRB5_KDB_INTERNAL_ERROR;
        goto done;
    }

    pentry = calloc(1, sizeof(osa_policy_ent_rec));
    if (!pentry) {
        kerr = ENOMEM;
        goto done;
    }
    pentry->version = 1;
    pentry->name = strdup(name);
    if (!pentry->name) {
        kerr = ENOMEM;
        goto done;
    }

    /* FIXME: what to do with missing attributes ? */

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbMinPwdLife", &result);
    if (ret == 0) {
        pentry->pw_min_life = result;
    }

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbMaxPwdLife", &result);
    if (ret == 0) {
        pentry->pw_max_life = result;
    }

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbPwdMinLength", &result);
    if (ret == 0) {
        pentry->pw_min_length = result;
    }

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbPwdMinDiffChars", &result);
    if (ret == 0) {
        pentry->pw_min_classes = result;
    }

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbPwdHistoryLength", &result);
    if (ret == 0) {
        pentry->pw_history_num = result;
    }

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbPwdMaxFailure", &result);
    if (ret == 0) {
        pentry->pw_max_fail = result;
    }

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbPwdFailureCountInterval", &result);
    if (ret == 0) {
        pentry->pw_failcnt_interval = result;
    }

    ret = ipadb_ldap_attr_to_uint32(ipactx->lcontext, lentry,
                                    "krbPwdLockoutDuration", &result);
    if (ret == 0) {
        pentry->pw_lockout_duration = result;
    }

    ret = ipa_kstuples_to_string(ipactx->supp_encs, ipactx->n_supp_encs,
                                 &pentry->allowed_keysalts);
    if (ret != 0) {
        kerr = KRB5_KDB_INTERNAL_ERROR;
        goto done;
    }

    *policy = pentry;

done:
    if (kerr) {
        free(pentry);
    }
    free(esc_name);
    free(src_filter);
    ldap_msgfree(res);

    return kerr;
}