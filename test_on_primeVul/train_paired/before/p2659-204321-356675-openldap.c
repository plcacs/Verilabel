ldap_X509dn2bv( void *x509_name, struct berval *bv, LDAPDN_rewrite_func *func,
	unsigned flags )
{
	LDAPDN	newDN;
	LDAPRDN	newRDN;
	LDAPAVA *newAVA, *baseAVA;
	BerElementBuffer berbuf;
	BerElement *ber = (BerElement *)&berbuf;
	char oids[8192], *oidptr = oids, *oidbuf = NULL;
	void *ptrs[2048];
	char *dn_end, *rdn_end;
	int i, navas, nrdns, rc = LDAP_SUCCESS;
	size_t dnsize, oidrem = sizeof(oids), oidsize = 0;
	int csize;
	ber_tag_t tag;
	ber_len_t len;
	oid_name *oidname;

	struct berval	Oid, Val, oid2, *in = x509_name;

	assert( bv != NULL );

	bv->bv_len = 0;
	bv->bv_val = NULL;

	navas = 0;
	nrdns = 0;

	/* A DN is a SEQUENCE of RDNs. An RDN is a SET of AVAs.
	 * An AVA is a SEQUENCE of attr and value.
	 * Count the number of AVAs and RDNs
	 */
	ber_init2( ber, in, LBER_USE_DER );
	tag = ber_peek_tag( ber, &len );
	if ( tag != LBER_SEQUENCE )
		return LDAP_DECODING_ERROR;

	for ( tag = ber_first_element( ber, &len, &dn_end );
		tag == LBER_SET;
		tag = ber_next_element( ber, &len, dn_end )) {
		nrdns++;
		for ( tag = ber_first_element( ber, &len, &rdn_end );
			tag == LBER_SEQUENCE;
			tag = ber_next_element( ber, &len, rdn_end )) {
			tag = ber_skip_tag( ber, &len );
			ber_skip_data( ber, len );
			navas++;
		}
	}

	/* Rewind and prepare to extract */
	ber_rewind( ber );
	tag = ber_first_element( ber, &len, &dn_end );
	if ( tag == LBER_DEFAULT )
		return LDAP_DECODING_ERROR;

	/* Allocate the DN/RDN/AVA stuff as a single block */    
	dnsize = sizeof(LDAPRDN) * (nrdns+1);
	dnsize += sizeof(LDAPAVA *) * (navas+nrdns);
	dnsize += sizeof(LDAPAVA) * navas;
	if (dnsize > sizeof(ptrs)) {
		newDN = (LDAPDN)LDAP_MALLOC( dnsize );
		if ( newDN == NULL )
			return LDAP_NO_MEMORY;
	} else {
		newDN = (LDAPDN)(char *)ptrs;
	}

	newDN[nrdns] = NULL;
	newRDN = (LDAPRDN)(newDN + nrdns+1);
	newAVA = (LDAPAVA *)(newRDN + navas + nrdns);
	baseAVA = newAVA;

	for ( i = nrdns - 1; i >= 0; i-- ) {
		newDN[i] = newRDN;

		for ( tag = ber_first_element( ber, &len, &rdn_end );
			tag == LBER_SEQUENCE;
			tag = ber_next_element( ber, &len, rdn_end )) {

			*newRDN++ = newAVA;
			tag = ber_skip_tag( ber, &len );
			tag = ber_get_stringbv( ber, &Oid, LBER_BV_NOTERM );
			if ( tag != LBER_TAG_OID ) {
				rc = LDAP_DECODING_ERROR;
				goto nomem;
			}

			oid2.bv_val = oidptr;
			oid2.bv_len = oidrem;
			if ( ber_decode_oid( &Oid, &oid2 ) < 0 ) {
				rc = LDAP_DECODING_ERROR;
				goto nomem;
			}
			oidname = find_oid( &oid2 );
			if ( !oidname ) {
				newAVA->la_attr = oid2;
				oidptr += oid2.bv_len + 1;
				oidrem -= oid2.bv_len + 1;

				/* Running out of OID buffer space? */
				if (oidrem < 128) {
					if ( oidsize == 0 ) {
						oidsize = sizeof(oids) * 2;
						oidrem = oidsize;
						oidbuf = LDAP_MALLOC( oidsize );
						if ( oidbuf == NULL ) goto nomem;
						oidptr = oidbuf;
					} else {
						char *old = oidbuf;
						oidbuf = LDAP_REALLOC( oidbuf, oidsize*2 );
						if ( oidbuf == NULL ) goto nomem;
						/* Buffer moved! Fix AVA pointers */
						if ( old != oidbuf ) {
							LDAPAVA *a;
							long dif = oidbuf - old;

							for (a=baseAVA; a<=newAVA; a++){
								if (a->la_attr.bv_val >= old &&
									a->la_attr.bv_val <= (old + oidsize))
									a->la_attr.bv_val += dif;
							}
						}
						oidptr = oidbuf + oidsize - oidrem;
						oidrem += oidsize;
						oidsize *= 2;
					}
				}
			} else {
				if ( func ) {
					newAVA->la_attr = oidname->oid;
				} else {
					newAVA->la_attr = oidname->name;
				}
			}
			newAVA->la_private = NULL;
			newAVA->la_flags = LDAP_AVA_STRING;
			tag = ber_get_stringbv( ber, &Val, LBER_BV_NOTERM );
			switch(tag) {
			case LBER_TAG_UNIVERSAL:
				/* This uses 32-bit ISO 10646-1 */
				csize = 4; goto to_utf8;
			case LBER_TAG_BMP:
				/* This uses 16-bit ISO 10646-1 */
				csize = 2; goto to_utf8;
			case LBER_TAG_TELETEX:
				/* This uses 8-bit, assume ISO 8859-1 */
				csize = 1;
to_utf8:		rc = ldap_ucs_to_utf8s( &Val, csize, &newAVA->la_value );
				newAVA->la_flags |= LDAP_AVA_NONPRINTABLE;
allocd:
				newAVA->la_flags |= LDAP_AVA_FREE_VALUE;
				if (rc != LDAP_SUCCESS) goto nomem;
				break;
			case LBER_TAG_UTF8:
				newAVA->la_flags |= LDAP_AVA_NONPRINTABLE;
				/* This is already in UTF-8 encoding */
			case LBER_TAG_IA5:
			case LBER_TAG_PRINTABLE:
				/* These are always 7-bit strings */
				newAVA->la_value = Val;
				break;
			case LBER_BITSTRING:
				/* X.690 bitString value converted to RFC4517 Bit String */
				rc = der_to_ldap_BitString( &Val, &newAVA->la_value );
				goto allocd;
			default:
				/* Not a string type at all */
				newAVA->la_flags = 0;
				newAVA->la_value = Val;
				break;
			}
			newAVA++;
		}
		*newRDN++ = NULL;
		tag = ber_next_element( ber, &len, dn_end );
	}
		
	if ( func ) {
		rc = func( newDN, flags, NULL );
		if ( rc != LDAP_SUCCESS )
			goto nomem;
	}

	rc = ldap_dn2bv_x( newDN, bv, LDAP_DN_FORMAT_LDAPV3, NULL );

nomem:
	for (;baseAVA < newAVA; baseAVA++) {
		if (baseAVA->la_flags & LDAP_AVA_FREE_ATTR)
			LDAP_FREE( baseAVA->la_attr.bv_val );
		if (baseAVA->la_flags & LDAP_AVA_FREE_VALUE)
			LDAP_FREE( baseAVA->la_value.bv_val );
	}

	if ( oidsize != 0 )
		LDAP_FREE( oidbuf );
	if ( newDN != (LDAPDN)(char *) ptrs )
		LDAP_FREE( newDN );
	return rc;
}