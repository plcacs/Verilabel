setup_server_realm(struct server_handle *handle, krb5_principal sprinc)
{
    kdc_realm_t         *newrealm;
    kdc_realm_t **kdc_realmlist = handle->kdc_realmlist;
    int kdc_numrealms = handle->kdc_numrealms;

    if (kdc_numrealms > 1) {
        if (!(newrealm = find_realm_data(handle, sprinc->realm.data,
                                         (krb5_ui_4) sprinc->realm.length)))
            return NULL;
        else
            return newrealm;
    }
    else
        return kdc_realmlist[0];
}