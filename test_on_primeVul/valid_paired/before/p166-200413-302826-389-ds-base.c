sync_create_state_control(Slapi_Entry *e, LDAPControl **ctrlp, int type, Sync_Cookie *cookie)
{
    int rc;
    BerElement *ber;
    struct berval *bvp;
    char *uuid;
    Slapi_Attr *attr;
    Slapi_Value *val;

    if (type == LDAP_SYNC_NONE || ctrlp == NULL || (ber = der_alloc()) == NULL) {
        return (LDAP_OPERATIONS_ERROR);
    }

    *ctrlp = NULL;

    slapi_entry_attr_find(e, SLAPI_ATTR_UNIQUEID, &attr);
    slapi_attr_first_value(attr, &val);
    uuid = sync_nsuniqueid2uuid(slapi_value_get_string(val));
    if ((rc = ber_printf(ber, "{eo", type, uuid, 16)) != -1) {
        if (cookie) {
            char *cookiestr = sync_cookie2str(cookie);
            rc = ber_printf(ber, "s}", cookiestr);
            slapi_ch_free((void **)&cookiestr);
        } else {
            rc = ber_printf(ber, "}");
        }
    }
    if (rc != -1) {
        rc = ber_flatten(ber, &bvp);
    }
    ber_free(ber, 1);
    slapi_ch_free((void **)&uuid);

    if (rc == -1) {
        return (LDAP_OPERATIONS_ERROR);
    }

    *ctrlp = (LDAPControl *)slapi_ch_malloc(sizeof(LDAPControl));
    (*ctrlp)->ldctl_iscritical = 0;
    (*ctrlp)->ldctl_oid = slapi_ch_strdup(LDAP_CONTROL_SYNC_STATE);
    (*ctrlp)->ldctl_value = *bvp; /* struct copy */

    bvp->bv_val = NULL;
    ber_bvfree(bvp);

    return (LDAP_SUCCESS);
}