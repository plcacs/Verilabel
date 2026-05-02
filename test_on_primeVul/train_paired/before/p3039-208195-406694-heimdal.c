_krb5_extract_ticket(krb5_context context,
		     krb5_kdc_rep *rep,
		     krb5_creds *creds,
		     krb5_keyblock *key,
		     krb5_const_pointer keyseed,
		     krb5_key_usage key_usage,
		     krb5_addresses *addrs,
		     unsigned nonce,
		     unsigned flags,
		     krb5_data *request,
		     krb5_decrypt_proc decrypt_proc,
		     krb5_const_pointer decryptarg)
{
    krb5_error_code ret;
    krb5_principal tmp_principal;
    size_t len = 0;
    time_t tmp_time;
    krb5_timestamp sec_now;

    /* decrypt */

    if (decrypt_proc == NULL)
	decrypt_proc = decrypt_tkt;

    ret = (*decrypt_proc)(context, key, key_usage, decryptarg, rep);
    if (ret)
	goto out;

    if (rep->enc_part.flags.enc_pa_rep && request) {
	krb5_crypto crypto = NULL;
	Checksum cksum;
	PA_DATA *pa = NULL;
	int idx = 0;

	_krb5_debug(context, 5, "processing enc-ap-rep");

	if (rep->enc_part.encrypted_pa_data == NULL ||
	    (pa = krb5_find_padata(rep->enc_part.encrypted_pa_data->val,
				   rep->enc_part.encrypted_pa_data->len,
				   KRB5_PADATA_REQ_ENC_PA_REP,
				   &idx)) == NULL)
	{
	    _krb5_debug(context, 5, "KRB5_PADATA_REQ_ENC_PA_REP missing");
	    ret = KRB5KRB_AP_ERR_MODIFIED;
	    goto out;
	}
	
	ret = krb5_crypto_init(context, key, 0, &crypto);
	if (ret)
	    goto out;
	
	ret = decode_Checksum(pa->padata_value.data,
			      pa->padata_value.length,
			      &cksum, NULL);
	if (ret) {
	    krb5_crypto_destroy(context, crypto);
	    goto out;
	}
	
	ret = krb5_verify_checksum(context, crypto,
				   KRB5_KU_AS_REQ,
				   request->data, request->length,
				   &cksum);
	krb5_crypto_destroy(context, crypto);
	free_Checksum(&cksum);
	_krb5_debug(context, 5, "enc-ap-rep: %svalid", (ret == 0) ? "" : "in");
	if (ret)
	    goto out;
    }

    /* save session key */

    creds->session.keyvalue.length = 0;
    creds->session.keyvalue.data   = NULL;
    creds->session.keytype = rep->enc_part.key.keytype;
    ret = krb5_data_copy (&creds->session.keyvalue,
			  rep->enc_part.key.keyvalue.data,
			  rep->enc_part.key.keyvalue.length);
    if (ret) {
	krb5_clear_error_message(context);
	goto out;
    }

    /* compare client and save */
    ret = _krb5_principalname2krb5_principal(context,
					     &tmp_principal,
					     rep->kdc_rep.cname,
					     rep->kdc_rep.crealm);
    if (ret)
	goto out;

    /* check client referral and save principal */
    /* anonymous here ? */
    if((flags & EXTRACT_TICKET_ALLOW_CNAME_MISMATCH) == 0) {
	ret = check_client_referral(context, rep,
				    creds->client,
				    tmp_principal,
				    &creds->session);
	if (ret) {
	    krb5_free_principal (context, tmp_principal);
	    goto out;
	}
    }
    krb5_free_principal (context, creds->client);
    creds->client = tmp_principal;

    /* check server referral and save principal */
    ret = _krb5_principalname2krb5_principal (context,
					      &tmp_principal,
					      rep->kdc_rep.ticket.sname,
					      rep->kdc_rep.ticket.realm);
    if (ret)
	goto out;
    if((flags & EXTRACT_TICKET_ALLOW_SERVER_MISMATCH) == 0){
	ret = check_server_referral(context,
				    rep,
				    flags,
				    creds->server,
				    tmp_principal,
				    &creds->session);
	if (ret) {
	    krb5_free_principal (context, tmp_principal);
	    goto out;
	}
    }
    krb5_free_principal(context, creds->server);
    creds->server = tmp_principal;

    /* verify names */
    if(flags & EXTRACT_TICKET_MATCH_REALM){
	const char *srealm = krb5_principal_get_realm(context, creds->server);
	const char *crealm = krb5_principal_get_realm(context, creds->client);

	if (strcmp(rep->enc_part.srealm, srealm) != 0 ||
	    strcmp(rep->enc_part.srealm, crealm) != 0)
	{
	    ret = KRB5KRB_AP_ERR_MODIFIED;
	    krb5_clear_error_message(context);
	    goto out;
	}
    }

    /* compare nonces */

    if (nonce != (unsigned)rep->enc_part.nonce) {
	ret = KRB5KRB_AP_ERR_MODIFIED;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto out;
    }

    /* set kdc-offset */

    krb5_timeofday (context, &sec_now);
    if (rep->enc_part.flags.initial
	&& (flags & EXTRACT_TICKET_TIMESYNC)
	&& context->kdc_sec_offset == 0
	&& krb5_config_get_bool (context, NULL,
				 "libdefaults",
				 "kdc_timesync",
				 NULL)) {
	context->kdc_sec_offset = rep->enc_part.authtime - sec_now;
	krb5_timeofday (context, &sec_now);
    }

    /* check all times */

    if (rep->enc_part.starttime) {
	tmp_time = *rep->enc_part.starttime;
    } else
	tmp_time = rep->enc_part.authtime;

    if (creds->times.starttime == 0
	&& labs(tmp_time - sec_now) > context->max_skew) {
	ret = KRB5KRB_AP_ERR_SKEW;
	krb5_set_error_message (context, ret,
				N_("time skew (%ld) larger than max (%ld)", ""),
			       labs(tmp_time - sec_now),
			       (long)context->max_skew);
	goto out;
    }

    if (creds->times.starttime != 0
	&& tmp_time != creds->times.starttime) {
	krb5_clear_error_message (context);
	ret = KRB5KRB_AP_ERR_MODIFIED;
	goto out;
    }

    creds->times.starttime = tmp_time;

    if (rep->enc_part.renew_till) {
	tmp_time = *rep->enc_part.renew_till;
    } else
	tmp_time = 0;

    if (creds->times.renew_till != 0
	&& tmp_time > creds->times.renew_till) {
	krb5_clear_error_message (context);
	ret = KRB5KRB_AP_ERR_MODIFIED;
	goto out;
    }

    creds->times.renew_till = tmp_time;

    creds->times.authtime = rep->enc_part.authtime;

    if (creds->times.endtime != 0
	&& rep->enc_part.endtime > creds->times.endtime) {
	krb5_clear_error_message (context);
	ret = KRB5KRB_AP_ERR_MODIFIED;
	goto out;
    }

    creds->times.endtime  = rep->enc_part.endtime;

    if(rep->enc_part.caddr)
	krb5_copy_addresses (context, rep->enc_part.caddr, &creds->addresses);
    else if(addrs)
	krb5_copy_addresses (context, addrs, &creds->addresses);
    else {
	creds->addresses.len = 0;
	creds->addresses.val = NULL;
    }
    creds->flags.b = rep->enc_part.flags;

    creds->authdata.len = 0;
    creds->authdata.val = NULL;

    /* extract ticket */
    ASN1_MALLOC_ENCODE(Ticket, creds->ticket.data, creds->ticket.length,
		       &rep->kdc_rep.ticket, &len, ret);
    if(ret)
	goto out;
    if (creds->ticket.length != len)
	krb5_abortx(context, "internal error in ASN.1 encoder");
    creds->second_ticket.length = 0;
    creds->second_ticket.data   = NULL;


out:
    memset (rep->enc_part.key.keyvalue.data, 0,
	    rep->enc_part.key.keyvalue.length);
    return ret;
}