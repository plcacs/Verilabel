issuerAndThisUpdateCheck(
	struct berval *in,
	struct berval *is,
	struct berval *tu,
	void *ctx )
{
	int numdquotes = 0;
	struct berval x = *in;
	struct berval ni = BER_BVNULL;
	/* Parse GSER format */ 
	enum {
		HAVE_NONE = 0x0,
		HAVE_ISSUER = 0x1,
		HAVE_THISUPDATE = 0x2,
		HAVE_ALL = ( HAVE_ISSUER | HAVE_THISUPDATE )
	} have = HAVE_NONE;


	if ( in->bv_len < STRLENOF( "{issuer \"\",thisUpdate \"YYMMDDhhmmssZ\"}" ) ) return LDAP_INVALID_SYNTAX;

	if ( in->bv_val[0] != '{' || in->bv_val[in->bv_len-1] != '}' ) {
		return LDAP_INVALID_SYNTAX;
	}

	x.bv_val++;
	x.bv_len -= STRLENOF("{}");

	do {
		/* eat leading spaces */
		for ( ; (x.bv_val[0] == ' ') && x.bv_len; x.bv_val++, x.bv_len-- ) {
			/* empty */;
		}

		/* should be at issuer or thisUpdate */
		if ( strncasecmp( x.bv_val, "issuer", STRLENOF("issuer") ) == 0 ) {
			if ( have & HAVE_ISSUER ) return LDAP_INVALID_SYNTAX;

			/* parse issuer */
			x.bv_val += STRLENOF("issuer");
			x.bv_len -= STRLENOF("issuer");

			if ( x.bv_val[0] != ' ' ) return LDAP_INVALID_SYNTAX;
			x.bv_val++;
			x.bv_len--;

			/* eat leading spaces */
			for ( ; (x.bv_val[0] == ' ') && x.bv_len; x.bv_val++, x.bv_len-- ) {
				/* empty */;
			}

			/* For backward compatibility, this part is optional */
			if ( strncasecmp( x.bv_val, "rdnSequence:", STRLENOF("rdnSequence:") ) != 0 ) {
				return LDAP_INVALID_SYNTAX;
			}
			x.bv_val += STRLENOF("rdnSequence:");
			x.bv_len -= STRLENOF("rdnSequence:");

			if ( x.bv_val[0] != '"' ) return LDAP_INVALID_SYNTAX;
			x.bv_val++;
			x.bv_len--;

			is->bv_val = x.bv_val;
			is->bv_len = 0;

			for ( ; is->bv_len < x.bv_len; ) {
				if ( is->bv_val[is->bv_len] != '"' ) {
					is->bv_len++;
					continue;
				}
				if ( is->bv_val[is->bv_len+1] == '"' ) {
					/* double dquote */
					numdquotes++;
					is->bv_len += 2;
					continue;
				}
				break;
			}
			x.bv_val += is->bv_len + 1;
			x.bv_len -= is->bv_len + 1;

			have |= HAVE_ISSUER;

		} else if ( strncasecmp( x.bv_val, "thisUpdate", STRLENOF("thisUpdate") ) == 0 )
		{
			if ( have & HAVE_THISUPDATE ) return LDAP_INVALID_SYNTAX;

			/* parse thisUpdate */
			x.bv_val += STRLENOF("thisUpdate");
			x.bv_len -= STRLENOF("thisUpdate");

			if ( x.bv_val[0] != ' ' ) return LDAP_INVALID_SYNTAX;
			x.bv_val++;
			x.bv_len--;

			/* eat leading spaces */
			for ( ; (x.bv_val[0] == ' ') && x.bv_len; x.bv_val++, x.bv_len-- ) {
				/* empty */;
			}

			if ( !x.bv_len || x.bv_val[0] != '"' ) return LDAP_INVALID_SYNTAX;
			x.bv_val++;
			x.bv_len--;

			tu->bv_val = x.bv_val;
			tu->bv_len = 0;

			for ( ; tu->bv_len < x.bv_len; tu->bv_len++ ) {
				if ( tu->bv_val[tu->bv_len] == '"' ) {
					break;
				}
			}
			x.bv_val += tu->bv_len + 1;
			x.bv_len -= tu->bv_len + 1;

			have |= HAVE_THISUPDATE;

		} else {
			return LDAP_INVALID_SYNTAX;
		}

		/* eat leading spaces */
		for ( ; (x.bv_val[0] == ' ') && x.bv_len; x.bv_val++, x.bv_len-- ) {
			/* empty */;
		}

		if ( have == HAVE_ALL ) {
			break;
		}

		if ( x.bv_val[0] != ',' ) {
			return LDAP_INVALID_SYNTAX;
		}

		x.bv_val++;
		x.bv_len--;
	} while ( 1 );

	/* should have no characters left... */
	if ( x.bv_len ) return LDAP_INVALID_SYNTAX;

	if ( numdquotes == 0 ) {
		ber_dupbv_x( &ni, is, ctx );

	} else {
		ber_len_t src, dst;

		ni.bv_len = is->bv_len - numdquotes;
		ni.bv_val = ber_memalloc_x( ni.bv_len + 1, ctx );
		for ( src = 0, dst = 0; src < is->bv_len; src++, dst++ ) {
			if ( is->bv_val[src] == '"' ) {
				src++;
			}
			ni.bv_val[dst] = is->bv_val[src];
		}
		ni.bv_val[dst] = '\0';
	}
		
	*is = ni;

	return 0;
}