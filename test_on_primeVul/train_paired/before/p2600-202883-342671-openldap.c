static int parseValuesReturnFilter (
	Operation *op,
	SlapReply *rs,
	LDAPControl *ctrl )
{
	BerElement	*ber;
	struct berval	fstr = BER_BVNULL;

	if ( op->o_valuesreturnfilter != SLAP_CONTROL_NONE ) {
		rs->sr_text = "valuesReturnFilter control specified multiple times";
		return LDAP_PROTOCOL_ERROR;
	}

	if ( BER_BVISNULL( &ctrl->ldctl_value )) {
		rs->sr_text = "valuesReturnFilter control value is absent";
		return LDAP_PROTOCOL_ERROR;
	}

	if ( BER_BVISEMPTY( &ctrl->ldctl_value )) {
		rs->sr_text = "valuesReturnFilter control value is empty";
		return LDAP_PROTOCOL_ERROR;
	}

	ber = ber_init( &(ctrl->ldctl_value) );
	if (ber == NULL) {
		rs->sr_text = "internal error";
		return LDAP_OTHER;
	}

	rs->sr_err = get_vrFilter( op, ber,
		(ValuesReturnFilter **)&(op->o_vrFilter), &rs->sr_text);

	(void) ber_free( ber, 1 );

	if( rs->sr_err != LDAP_SUCCESS ) {
		if( rs->sr_err == SLAPD_DISCONNECT ) {
			rs->sr_err = LDAP_PROTOCOL_ERROR;
			send_ldap_disconnect( op, rs );
			rs->sr_err = SLAPD_DISCONNECT;
		} else {
			send_ldap_result( op, rs );
		}
		if( op->o_vrFilter != NULL) vrFilter_free( op, op->o_vrFilter ); 
	}
#ifdef LDAP_DEBUG
	else {
		vrFilter2bv( op, op->o_vrFilter, &fstr );
	}

	Debug( LDAP_DEBUG_ARGS, "	vrFilter: %s\n",
		fstr.bv_len ? fstr.bv_val : "empty", 0, 0 );
	op->o_tmpfree( fstr.bv_val, op->o_tmpmemctx );
#endif

	op->o_valuesreturnfilter = ctrl->ldctl_iscritical
		? SLAP_CONTROL_CRITICAL
		: SLAP_CONTROL_NONCRITICAL;

	rs->sr_err = LDAP_SUCCESS;
	return LDAP_SUCCESS;
}