certificateListValidate( Syntax *syntax, struct berval *in )
{
	BerElementBuffer berbuf;
	BerElement *ber = (BerElement *)&berbuf;
	ber_tag_t tag;
	ber_len_t len, wrapper_len;
	char *wrapper_start;
	int wrapper_ok = 0;
	ber_int_t version = SLAP_X509_V1;
	struct berval bvdn, bvtu;

	ber_init2( ber, in, LBER_USE_DER );
	tag = ber_skip_tag( ber, &wrapper_len );	/* Signed wrapper */
	if ( tag != LBER_SEQUENCE ) return LDAP_INVALID_SYNTAX;
	wrapper_start = ber->ber_ptr;
	tag = ber_skip_tag( ber, &len );	/* Sequence */
	if ( tag != LBER_SEQUENCE ) return LDAP_INVALID_SYNTAX;
	tag = ber_peek_tag( ber, &len );
	/* Optional version */
	if ( tag == LBER_INTEGER ) {
		tag = ber_get_int( ber, &version );
		assert( tag == LBER_INTEGER );
		if ( version != SLAP_X509_V2 ) return LDAP_INVALID_SYNTAX;
	}
	tag = ber_skip_tag( ber, &len );	/* Signature Algorithm */
	if ( tag != LBER_SEQUENCE ) return LDAP_INVALID_SYNTAX;
	ber_skip_data( ber, len );
	tag = ber_peek_tag( ber, &len );	/* Issuer DN */
	if ( tag != LBER_SEQUENCE ) return LDAP_INVALID_SYNTAX;
	len = ber_ptrlen( ber );
	bvdn.bv_val = in->bv_val + len;
	bvdn.bv_len = in->bv_len - len;
	tag = ber_skip_tag( ber, &len );
	ber_skip_data( ber, len );
	tag = ber_skip_tag( ber, &len );	/* thisUpdate */
	/* Time is a CHOICE { UTCTime, GeneralizedTime } */
	if ( tag != SLAP_TAG_UTCTIME && tag != SLAP_TAG_GENERALIZEDTIME ) return LDAP_INVALID_SYNTAX;
	bvtu.bv_val = (char *)ber->ber_ptr;
	bvtu.bv_len = len;
	ber_skip_data( ber, len );
	/* Optional nextUpdate */
	tag = ber_skip_tag( ber, &len );
	if ( tag == SLAP_TAG_UTCTIME || tag == SLAP_TAG_GENERALIZEDTIME ) {
		ber_skip_data( ber, len );
		tag = ber_skip_tag( ber, &len );
	}
	/* revokedCertificates - Sequence of Sequence, Optional */
	if ( tag == LBER_SEQUENCE ) {
		ber_len_t seqlen;
		ber_tag_t stag;
		stag = ber_peek_tag( ber, &seqlen );
		if ( stag == LBER_SEQUENCE || !len ) {
			/* RFC5280 requires non-empty, but X.509(2005) allows empty. */
			if ( len )
				ber_skip_data( ber, len );
			tag = ber_skip_tag( ber, &len );
		}
	}
	/* Optional Extensions - Sequence of Sequence */
	if ( tag == SLAP_X509_OPT_CL_CRLEXTENSIONS ) { /* ? */
		ber_len_t seqlen;
		if ( version != SLAP_X509_V2 ) return LDAP_INVALID_SYNTAX;
		tag = ber_peek_tag( ber, &seqlen );
		if ( tag != LBER_SEQUENCE ) return LDAP_INVALID_SYNTAX;
		ber_skip_data( ber, len );
		tag = ber_skip_tag( ber, &len );
	}
	/* signatureAlgorithm */
	if ( tag != LBER_SEQUENCE ) return LDAP_INVALID_SYNTAX;
	ber_skip_data( ber, len );
	tag = ber_skip_tag( ber, &len );
	/* Signature */
	if ( tag != LBER_BITSTRING ) return LDAP_INVALID_SYNTAX; 
	ber_skip_data( ber, len );
	if ( ber->ber_ptr == wrapper_start + wrapper_len ) wrapper_ok = 1;
	tag = ber_skip_tag( ber, &len );
	/* Must be at end now */
	/* NOTE: OpenSSL tolerates CL with garbage past the end */
	if ( len || tag != LBER_DEFAULT ) {
		struct berval issuer_dn = BER_BVNULL, thisUpdate;
		char tubuf[STRLENOF("YYYYmmddHHMMSSZ") + 1];
		int rc;

		if ( ! wrapper_ok ) {
			return LDAP_INVALID_SYNTAX;
		}

		rc = dnX509normalize( &bvdn, &issuer_dn );
		if ( rc != LDAP_SUCCESS ) {
			rc = LDAP_INVALID_SYNTAX;
			goto done;
		}

		thisUpdate.bv_val = tubuf;
		thisUpdate.bv_len = sizeof(tubuf); 
		if ( checkTime( &bvtu, &thisUpdate ) ) {
			rc = LDAP_INVALID_SYNTAX;
			goto done;
		}

		Debug( LDAP_DEBUG_ANY,
			"certificateListValidate issuer=\"%s\", thisUpdate=%s: extra cruft past end of certificateList\n",
			issuer_dn.bv_val, thisUpdate.bv_val, 0 );

done:;
		if ( ! BER_BVISNULL( &issuer_dn ) ) {
			ber_memfree( issuer_dn.bv_val );
		}

		return rc;
	}

	return LDAP_SUCCESS;
}