authzPrettyNormal(
	struct berval	*val,
	struct berval	*normalized,
	void		*ctx,
	int		normalize )
{
	struct berval	bv;
	int		rc = LDAP_INVALID_SYNTAX;
	LDAPURLDesc	*ludp = NULL;
	char		*lud_dn = NULL,
			*lud_filter = NULL;
	int		scope = -1;

	/*
	 * 1) <DN>
	 * 2) dn[.{exact|children|subtree|onelevel}]:{*|<DN>}
	 * 3) dn.regex:<pattern>
	 * 4) u[.mech[/realm]]:<ID>
	 * 5) group[/<groupClass>[/<memberAttr>]]:<DN>
	 * 6) <URL>
	 */

	assert( val != NULL );
	assert( !BER_BVISNULL( val ) );
	BER_BVZERO( normalized );

	/*
	 * 2) dn[.{exact|children|subtree|onelevel}]:{*|<DN>}
	 * 3) dn.regex:<pattern>
	 *
	 * <DN> must pass DN normalization
	 */
	if ( !strncasecmp( val->bv_val, "dn", STRLENOF( "dn" ) ) ) {
		struct berval	out = BER_BVNULL,
				prefix = BER_BVNULL;
		char		*ptr;

		bv.bv_val = val->bv_val + STRLENOF( "dn" );

		if ( bv.bv_val[ 0 ] == '.' ) {
			bv.bv_val++;

			if ( !strncasecmp( bv.bv_val, "exact:", STRLENOF( "exact:" ) ) ) {
				bv.bv_val += STRLENOF( "exact:" );
				scope = LDAP_X_SCOPE_EXACT;

			} else if ( !strncasecmp( bv.bv_val, "regex:", STRLENOF( "regex:" ) ) ) {
				bv.bv_val += STRLENOF( "regex:" );
				scope = LDAP_X_SCOPE_REGEX;

			} else if ( !strncasecmp( bv.bv_val, "children:", STRLENOF( "children:" ) ) ) {
				bv.bv_val += STRLENOF( "children:" );
				scope = LDAP_X_SCOPE_CHILDREN;

			} else if ( !strncasecmp( bv.bv_val, "subtree:", STRLENOF( "subtree:" ) ) ) {
				bv.bv_val += STRLENOF( "subtree:" );
				scope = LDAP_X_SCOPE_SUBTREE;

			} else if ( !strncasecmp( bv.bv_val, "onelevel:", STRLENOF( "onelevel:" ) ) ) {
				bv.bv_val += STRLENOF( "onelevel:" );
				scope = LDAP_X_SCOPE_ONELEVEL;

			} else {
				return LDAP_INVALID_SYNTAX;
			}

		} else {
			if ( bv.bv_val[ 0 ] != ':' ) {
				return LDAP_INVALID_SYNTAX;
			}
			scope = LDAP_X_SCOPE_EXACT;
			bv.bv_val++;
		}

		bv.bv_val += strspn( bv.bv_val, " " );
		/* jump here in case no type specification was present
		 * and uri was not an URI... HEADS-UP: assuming EXACT */
is_dn:		bv.bv_len = val->bv_len - ( bv.bv_val - val->bv_val );

		/* a single '*' means any DN without using regexes */
		if ( ber_bvccmp( &bv, '*' ) ) {
			ber_str2bv_x( "dn:*", STRLENOF( "dn:*" ), 1, normalized, ctx );
			return LDAP_SUCCESS;
		}

		switch ( scope ) {
		case LDAP_X_SCOPE_EXACT:
		case LDAP_X_SCOPE_CHILDREN:
		case LDAP_X_SCOPE_SUBTREE:
		case LDAP_X_SCOPE_ONELEVEL:
			if ( normalize ) {
				rc = dnNormalize( 0, NULL, NULL, &bv, &out, ctx );
			} else {
				rc = dnPretty( NULL, &bv, &out, ctx );
			}
			if( rc != LDAP_SUCCESS ) {
				return LDAP_INVALID_SYNTAX;
			}
			break;

		case LDAP_X_SCOPE_REGEX:
			normalized->bv_len = STRLENOF( "dn.regex:" ) + bv.bv_len;
			normalized->bv_val = ber_memalloc_x( normalized->bv_len + 1, ctx );
			ptr = lutil_strcopy( normalized->bv_val, "dn.regex:" );
			ptr = lutil_strncopy( ptr, bv.bv_val, bv.bv_len );
			ptr[ 0 ] = '\0';
			return LDAP_SUCCESS;

		default:
			return LDAP_INVALID_SYNTAX;
		}

		/* prepare prefix */
		switch ( scope ) {
		case LDAP_X_SCOPE_EXACT:
			BER_BVSTR( &prefix, "dn:" );
			break;

		case LDAP_X_SCOPE_CHILDREN:
			BER_BVSTR( &prefix, "dn.children:" );
			break;

		case LDAP_X_SCOPE_SUBTREE:
			BER_BVSTR( &prefix, "dn.subtree:" );
			break;

		case LDAP_X_SCOPE_ONELEVEL:
			BER_BVSTR( &prefix, "dn.onelevel:" );
			break;

		default:
			assert( 0 );
			break;
		}

		normalized->bv_len = prefix.bv_len + out.bv_len;
		normalized->bv_val = ber_memalloc_x( normalized->bv_len + 1, ctx );
		
		ptr = lutil_strcopy( normalized->bv_val, prefix.bv_val );
		ptr = lutil_strncopy( ptr, out.bv_val, out.bv_len );
		ptr[ 0 ] = '\0';
		ber_memfree_x( out.bv_val, ctx );

		return LDAP_SUCCESS;

	/*
	 * 4) u[.mech[/realm]]:<ID>
	 */
	} else if ( ( val->bv_val[ 0 ] == 'u' || val->bv_val[ 0 ] == 'U' )
			&& ( val->bv_val[ 1 ] == ':' 
				|| val->bv_val[ 1 ] == '/' 
				|| val->bv_val[ 1 ] == '.' ) )
	{
		char		buf[ SLAP_LDAPDN_MAXLEN ];
		struct berval	id,
				user = BER_BVNULL,
				realm = BER_BVNULL,
				mech = BER_BVNULL;

		if ( sizeof( buf ) <= val->bv_len ) {
			return LDAP_INVALID_SYNTAX;
		}

		id.bv_len = val->bv_len;
		id.bv_val = buf;
		strncpy( buf, val->bv_val, sizeof( buf ) );

		rc = slap_parse_user( &id, &user, &realm, &mech );
		if ( rc != LDAP_SUCCESS ) {
			return LDAP_INVALID_SYNTAX;
		}

		ber_dupbv_x( normalized, val, ctx );

		return rc;

	/*
	 * 5) group[/groupClass[/memberAttr]]:<DN>
	 *
	 * <groupClass> defaults to "groupOfNames"
	 * <memberAttr> defaults to "member"
	 * 
	 * <DN> must pass DN normalization
	 */
	} else if ( strncasecmp( val->bv_val, "group", STRLENOF( "group" ) ) == 0 )
	{
		struct berval	group_dn = BER_BVNULL,
				group_oc = BER_BVNULL,
				member_at = BER_BVNULL,
				out = BER_BVNULL;
		char		*ptr;

		bv.bv_val = val->bv_val + STRLENOF( "group" );
		bv.bv_len = val->bv_len - STRLENOF( "group" );
		group_dn.bv_val = ber_bvchr( &bv, ':' );
		if ( group_dn.bv_val == NULL ) {
			/* last chance: assume it's a(n exact) DN ... */
			bv.bv_val = val->bv_val;
			scope = LDAP_X_SCOPE_EXACT;
			goto is_dn;
		}

		/*
		 * FIXME: we assume that "member" and "groupOfNames"
		 * are present in schema...
		 */
		if ( bv.bv_val[ 0 ] == '/' ) {
			ObjectClass		*oc = NULL;

			group_oc.bv_val = &bv.bv_val[ 1 ];
			group_oc.bv_len = group_dn.bv_val - group_oc.bv_val;

			member_at.bv_val = ber_bvchr( &group_oc, '/' );
			if ( member_at.bv_val ) {
				AttributeDescription	*ad = NULL;
				const char		*text = NULL;

				group_oc.bv_len = member_at.bv_val - group_oc.bv_val;
				member_at.bv_val++;
				member_at.bv_len = group_dn.bv_val - member_at.bv_val;
				rc = slap_bv2ad( &member_at, &ad, &text );
				if ( rc != LDAP_SUCCESS ) {
					return rc;
				}

				member_at = ad->ad_cname;

			}

			oc = oc_bvfind( &group_oc );
			if ( oc == NULL ) {
				return LDAP_INVALID_SYNTAX;
			}

			group_oc = oc->soc_cname;
		}

		group_dn.bv_val++;
		group_dn.bv_len = val->bv_len - ( group_dn.bv_val - val->bv_val );

		if ( normalize ) {
			rc = dnNormalize( 0, NULL, NULL, &group_dn, &out, ctx );
		} else {
			rc = dnPretty( NULL, &group_dn, &out, ctx );
		}
		if ( rc != LDAP_SUCCESS ) {
			return rc;
		}

		normalized->bv_len = STRLENOF( "group" ":" ) + out.bv_len;
		if ( !BER_BVISNULL( &group_oc ) ) {
			normalized->bv_len += STRLENOF( "/" ) + group_oc.bv_len;
			if ( !BER_BVISNULL( &member_at ) ) {
				normalized->bv_len += STRLENOF( "/" ) + member_at.bv_len;
			}
		}

		normalized->bv_val = ber_memalloc_x( normalized->bv_len + 1, ctx );
		ptr = lutil_strcopy( normalized->bv_val, "group" );
		if ( !BER_BVISNULL( &group_oc ) ) {
			ptr[ 0 ] = '/';
			ptr++;
			ptr = lutil_strncopy( ptr, group_oc.bv_val, group_oc.bv_len );
			if ( !BER_BVISNULL( &member_at ) ) {
				ptr[ 0 ] = '/';
				ptr++;
				ptr = lutil_strncopy( ptr, member_at.bv_val, member_at.bv_len );
			}
		}
		ptr[ 0 ] = ':';
		ptr++;
		ptr = lutil_strncopy( ptr, out.bv_val, out.bv_len );
		ptr[ 0 ] = '\0';
		ber_memfree_x( out.bv_val, ctx );

		return rc;
	}

	/*
	 * ldap:///<base>??<scope>?<filter>
	 * <scope> ::= {base|one|subtree}
	 *
	 * <scope> defaults to "base"
	 * <base> must pass DN normalization
	 * <filter> must pass str2filter()
	 */
	rc = ldap_url_parse( val->bv_val, &ludp );
	switch ( rc ) {
	case LDAP_URL_SUCCESS:
		/* FIXME: the check is pedantic, but I think it's necessary,
		 * because people tend to use things like ldaps:// which
		 * gives the idea SSL is being used.  Maybe we could
		 * accept ldapi:// as well, but the point is that we use
		 * an URL as an easy means to define bits of a search with
		 * little parsing.
		 */
		if ( strcasecmp( ludp->lud_scheme, "ldap" ) != 0 ) {
			/*
			 * must be ldap:///
			 */
			rc = LDAP_INVALID_SYNTAX;
			goto done;
		}

		AC_MEMCPY( ludp->lud_scheme, "ldap", STRLENOF( "ldap" ) );
		break;

	case LDAP_URL_ERR_BADSCHEME:
		/*
		 * last chance: assume it's a(n exact) DN ...
		 *
		 * NOTE: must pass DN normalization
		 */
		ldap_free_urldesc( ludp );
		bv.bv_val = val->bv_val;
		scope = LDAP_X_SCOPE_EXACT;
		goto is_dn;

	default:
		rc = LDAP_INVALID_SYNTAX;
		goto done;
	}

	if ( ( ludp->lud_host && *ludp->lud_host )
		|| ludp->lud_attrs || ludp->lud_exts )
	{
		/* host part must be empty */
		/* attrs and extensions parts must be empty */
		rc = LDAP_INVALID_SYNTAX;
		goto done;
	}

	/* Grab the filter */
	if ( ludp->lud_filter ) {
		struct berval	filterstr;
		Filter		*f;

		lud_filter = ludp->lud_filter;

		f = str2filter( lud_filter );
		if ( f == NULL ) {
			rc = LDAP_INVALID_SYNTAX;
			goto done;
		}
		filter2bv( f, &filterstr );
		filter_free( f );
		if ( BER_BVISNULL( &filterstr ) ) {
			rc = LDAP_INVALID_SYNTAX;
			goto done;
		}

		ludp->lud_filter = filterstr.bv_val;
	}

	/* Grab the searchbase */
	if ( ludp->lud_dn ) {
		struct berval	out = BER_BVNULL;

		lud_dn = ludp->lud_dn;

		ber_str2bv( lud_dn, 0, 0, &bv );
		if ( normalize ) {
			rc = dnNormalize( 0, NULL, NULL, &bv, &out, ctx );
		} else {
			rc = dnPretty( NULL, &bv, &out, ctx );
		}

		if ( rc != LDAP_SUCCESS ) {
			goto done;
		}

		ludp->lud_dn = out.bv_val;
	} else {
		rc = LDAP_INVALID_SYNTAX;
		goto done;
	}

	ludp->lud_port = 0;
	normalized->bv_val = ldap_url_desc2str( ludp );
	if ( normalized->bv_val ) {
		normalized->bv_len = strlen( normalized->bv_val );

	} else {
		rc = LDAP_INVALID_SYNTAX;
	}

done:
	if ( lud_filter ) {
		if ( ludp->lud_filter != lud_filter ) {
			ber_memfree( ludp->lud_filter );
		}
		ludp->lud_filter = lud_filter;
	}

	if ( lud_dn ) {
		if ( ludp->lud_dn != lud_dn ) {
			slap_sl_free( ludp->lud_dn, ctx );
		}
		ludp->lud_dn = lud_dn;
	}

	ldap_free_urldesc( ludp );

	return( rc );
}