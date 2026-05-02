usm_free_usmStateReference(void *old)
{
    struct usmStateReference *ref = old;

    if (!ref)
        return;

    if (--ref->refcnt > 0)
        return;

    SNMP_FREE(ref->usr_name);
    SNMP_FREE(ref->usr_engine_id);
    SNMP_FREE(ref->usr_auth_protocol);
    SNMP_FREE(ref->usr_priv_protocol);

    if (ref->usr_auth_key_length && ref->usr_auth_key) {
        SNMP_ZERO(ref->usr_auth_key, ref->usr_auth_key_length);
        SNMP_FREE(ref->usr_auth_key);
    }
    if (ref->usr_priv_key_length && ref->usr_priv_key) {
        SNMP_ZERO(ref->usr_priv_key, ref->usr_priv_key_length);
        SNMP_FREE(ref->usr_priv_key);
    }

    SNMP_FREE(ref);
}                               /* end usm_free_usmStateReference() */