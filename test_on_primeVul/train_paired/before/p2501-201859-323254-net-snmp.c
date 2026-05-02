usm_free_usmStateReference(void *old)
{
    struct usmStateReference *old_ref = (struct usmStateReference *) old;

    if (old_ref) {

        if (old_ref->usr_name_length)
            SNMP_FREE(old_ref->usr_name);
        if (old_ref->usr_engine_id_length)
            SNMP_FREE(old_ref->usr_engine_id);
        if (old_ref->usr_auth_protocol_length)
            SNMP_FREE(old_ref->usr_auth_protocol);
        if (old_ref->usr_priv_protocol_length)
            SNMP_FREE(old_ref->usr_priv_protocol);

        if (old_ref->usr_auth_key_length && old_ref->usr_auth_key) {
            SNMP_ZERO(old_ref->usr_auth_key, old_ref->usr_auth_key_length);
            SNMP_FREE(old_ref->usr_auth_key);
        }
        if (old_ref->usr_priv_key_length && old_ref->usr_priv_key) {
            SNMP_ZERO(old_ref->usr_priv_key, old_ref->usr_priv_key_length);
            SNMP_FREE(old_ref->usr_priv_key);
        }

        SNMP_ZERO(old_ref, sizeof(*old_ref));
        SNMP_FREE(old_ref);

    }

}                               /* end usm_free_usmStateReference() */