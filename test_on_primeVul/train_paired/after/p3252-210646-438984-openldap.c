int slap_parse_user( struct berval *id, struct berval *user,
		struct berval *realm, struct berval *mech )
{
	char	u;
	
	assert( id != NULL );
	assert( !BER_BVISNULL( id ) );
	assert( user != NULL );
	assert( realm != NULL );
	assert( mech != NULL );

	u = id->bv_val[ 0 ];
	
	if ( u != 'u' && u != 'U' ) {
		/* called with something other than u: */
		return LDAP_PROTOCOL_ERROR;
	}

	/* uauthzid form:
	 *		u[.mech[/realm]]:user
	 */
	
	user->bv_val = ber_bvchr( id, ':' );
	if ( BER_BVISNULL( user ) ) {
		return LDAP_PROTOCOL_ERROR;
	}
	user->bv_val[ 0 ] = '\0';
	user->bv_val++;
	user->bv_len = id->bv_len - ( user->bv_val - id->bv_val );

	if ( id->bv_val[1] == '.' ) {
		id->bv_val[1] = '\0';
		mech->bv_val = id->bv_val + 2;
		mech->bv_len = user->bv_val - mech->bv_val - 1;

		realm->bv_val = ber_bvchr( mech, '/' );

		if ( !BER_BVISNULL( realm ) ) {
			realm->bv_val[ 0 ] = '\0';
			realm->bv_val++;
			mech->bv_len = realm->bv_val - mech->bv_val - 1;
			realm->bv_len = user->bv_val - realm->bv_val - 1;
		}

	} else {
		BER_BVZERO( mech );
		BER_BVZERO( realm );
	}

	if ( id->bv_val[ 1 ] != '\0' ) {
		return LDAP_PROTOCOL_ERROR;
	}

	if ( !BER_BVISNULL( mech ) ) {
		if ( mech->bv_val != id->bv_val + 2 )
			return LDAP_PROTOCOL_ERROR;

		AC_MEMCPY( mech->bv_val - 2, mech->bv_val, mech->bv_len + 1 );
		mech->bv_val -= 2;
	}

	if ( !BER_BVISNULL( realm ) ) {
		if ( realm->bv_val < id->bv_val + 2 )
			return LDAP_PROTOCOL_ERROR;

		AC_MEMCPY( realm->bv_val - 2, realm->bv_val, realm->bv_len + 1 );
		realm->bv_val -= 2;
	}

	/* leave "u:" before user */
	user->bv_val -= 2;
	user->bv_len += 2;
	user->bv_val[ 0 ] = u;
	user->bv_val[ 1 ] = ':';

	return LDAP_SUCCESS;
}