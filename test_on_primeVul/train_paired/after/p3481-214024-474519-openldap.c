AVA_Sort( LDAPRDN rdn, int nAVAs )
{
	LDAPAVA	*ava_i;
	int		i;
	int		rc = LDAP_SUCCESS;

	assert( rdn != NULL );

	for ( i = 1; i < nAVAs; i++ ) {
		LDAPAVA *ava_j;
		int j;

		ava_i = rdn[ i ];
		for ( j = i-1; j >=0; j-- ) {
			int a;

			ava_j = rdn[ j ];
			a = strcmp( ava_i->la_attr.bv_val, ava_j->la_attr.bv_val );

			/* RFC4512 does not allow multiple AVAs
			 * with the same attribute type in RDN (ITS#5968) */
			if ( a == 0 )
				rc = LDAP_INVALID_DN_SYNTAX;

			if ( a > 0 )
				break;

			rdn[ j+1 ] = rdn[ j ];
		}
		rdn[ j+1 ] = ava_i;
	}
	return rc;
}