slap_modrdn2mods(
	Operation	*op,
	SlapReply	*rs )
{
	int		a_cnt, d_cnt;
	LDAPRDN		old_rdn = NULL;
	LDAPRDN		new_rdn = NULL;

	assert( !BER_BVISEMPTY( &op->oq_modrdn.rs_newrdn ) );

	/* if requestDN is empty, silently reset deleteOldRDN */
	if ( BER_BVISEMPTY( &op->o_req_dn ) ) op->orr_deleteoldrdn = 0;

	if ( ldap_bv2rdn_x( &op->oq_modrdn.rs_newrdn, &new_rdn,
		(char **)&rs->sr_text, LDAP_DN_FORMAT_LDAP, op->o_tmpmemctx ) ) {
		Debug( LDAP_DEBUG_TRACE,
			"%s slap_modrdn2mods: can't figure out "
			"type(s)/value(s) of newrdn\n",
			op->o_log_prefix );
		rs->sr_err = LDAP_INVALID_DN_SYNTAX;
		rs->sr_text = "unknown type(s)/value(s) used in RDN";
		goto done;
	}

	if ( op->oq_modrdn.rs_deleteoldrdn ) {
		if ( ldap_bv2rdn_x( &op->o_req_dn, &old_rdn,
			(char **)&rs->sr_text, LDAP_DN_FORMAT_LDAP, op->o_tmpmemctx ) ) {
			Debug( LDAP_DEBUG_TRACE,
				"%s slap_modrdn2mods: can't figure out "
				"type(s)/value(s) of oldrdn\n",
				op->o_log_prefix );
			rs->sr_err = LDAP_OTHER;
			rs->sr_text = "cannot parse RDN from old DN";
			goto done;
		}
	}
	rs->sr_text = NULL;

	/* Add new attribute values to the entry */
	for ( a_cnt = 0; new_rdn[a_cnt]; a_cnt++ ) {
		AttributeDescription	*desc = NULL;
		Modifications 		*mod_tmp;

		rs->sr_err = slap_bv2ad( &new_rdn[a_cnt]->la_attr, &desc, &rs->sr_text );

		if ( rs->sr_err != LDAP_SUCCESS ) {
			Debug( LDAP_DEBUG_TRACE,
				"%s slap_modrdn2mods: %s: %s (new)\n",
				op->o_log_prefix,
				rs->sr_text,
				new_rdn[ a_cnt ]->la_attr.bv_val );
			goto done;		
		}

		if ( !desc->ad_type->sat_equality ) {
			Debug( LDAP_DEBUG_TRACE,
				"%s slap_modrdn2mods: %s: %s (new)\n",
				op->o_log_prefix,
				rs->sr_text,
				new_rdn[ a_cnt ]->la_attr.bv_val );
			rs->sr_text = "naming attribute has no equality matching rule";
			rs->sr_err = LDAP_NAMING_VIOLATION;
			goto done;
		}

		/* Apply modification */
		mod_tmp = ( Modifications * )ch_malloc( sizeof( Modifications ) );
		mod_tmp->sml_desc = desc;
		BER_BVZERO( &mod_tmp->sml_type );
		mod_tmp->sml_numvals = 1;
		mod_tmp->sml_values = ( BerVarray )ch_malloc( 2 * sizeof( struct berval ) );
		ber_dupbv( &mod_tmp->sml_values[0], &new_rdn[a_cnt]->la_value );
		mod_tmp->sml_values[1].bv_val = NULL;
		if( desc->ad_type->sat_equality->smr_normalize) {
			mod_tmp->sml_nvalues = ( BerVarray )ch_malloc( 2 * sizeof( struct berval ) );
			rs->sr_err = desc->ad_type->sat_equality->smr_normalize(
				SLAP_MR_EQUALITY|SLAP_MR_VALUE_OF_ASSERTION_SYNTAX,
				desc->ad_type->sat_syntax,
				desc->ad_type->sat_equality,
				&mod_tmp->sml_values[0],
				&mod_tmp->sml_nvalues[0], NULL );
			if (rs->sr_err != LDAP_SUCCESS) {
				ch_free(mod_tmp->sml_nvalues);
				ch_free(mod_tmp->sml_values[0].bv_val);
				ch_free(mod_tmp->sml_values);
				ch_free(mod_tmp);
				goto done;
			}
			mod_tmp->sml_nvalues[1].bv_val = NULL;
		} else {
			mod_tmp->sml_nvalues = NULL;
		}
		mod_tmp->sml_op = SLAP_MOD_SOFTADD;
		mod_tmp->sml_flags = 0;
		mod_tmp->sml_next = op->orr_modlist;
		op->orr_modlist = mod_tmp;
	}

	/* Remove old rdn value if required */
	if ( op->orr_deleteoldrdn ) {
		for ( d_cnt = 0; old_rdn[d_cnt]; d_cnt++ ) {
			AttributeDescription	*desc = NULL;
			Modifications 		*mod_tmp;

			rs->sr_err = slap_bv2ad( &old_rdn[d_cnt]->la_attr, &desc, &rs->sr_text );
			if ( rs->sr_err != LDAP_SUCCESS ) {
				Debug( LDAP_DEBUG_TRACE,
					"%s slap_modrdn2mods: %s: %s (old)\n",
					op->o_log_prefix,
					rs->sr_text, 
					old_rdn[d_cnt]->la_attr.bv_val );
				goto done;		
			}
			if ( !desc->ad_type->sat_equality ) {
				Debug( LDAP_DEBUG_TRACE,
					"%s slap_modrdn2mods: %s: %s (old)\n",
					op->o_log_prefix,
					rs->sr_text,
					old_rdn[ d_cnt ]->la_attr.bv_val );
				rs->sr_text = "naming attribute has no equality matching rule";
				rs->sr_err = LDAP_NAMING_VIOLATION;
				goto done;
			}

			/* Apply modification */
			mod_tmp = ( Modifications * )ch_malloc( sizeof( Modifications ) );
			mod_tmp->sml_desc = desc;
			BER_BVZERO( &mod_tmp->sml_type );
			mod_tmp->sml_numvals = 1;
			mod_tmp->sml_values = ( BerVarray )ch_malloc( 2 * sizeof( struct berval ) );
			ber_dupbv( &mod_tmp->sml_values[0], &old_rdn[d_cnt]->la_value );
			mod_tmp->sml_values[1].bv_val = NULL;
			if( desc->ad_type->sat_equality->smr_normalize) {
				mod_tmp->sml_nvalues = ( BerVarray )ch_malloc( 2 * sizeof( struct berval ) );
				(void) (*desc->ad_type->sat_equality->smr_normalize)(
					SLAP_MR_EQUALITY|SLAP_MR_VALUE_OF_ASSERTION_SYNTAX,
					desc->ad_type->sat_syntax,
					desc->ad_type->sat_equality,
					&mod_tmp->sml_values[0],
					&mod_tmp->sml_nvalues[0], NULL );
				mod_tmp->sml_nvalues[1].bv_val = NULL;
			} else {
				mod_tmp->sml_nvalues = NULL;
			}
			mod_tmp->sml_op = LDAP_MOD_DELETE;
			mod_tmp->sml_flags = 0;
			mod_tmp->sml_next = op->orr_modlist;
			op->orr_modlist = mod_tmp;
		}
	}
	
done:

	/* LDAP v2 supporting correct attribute handling. */
	if ( rs->sr_err != LDAP_SUCCESS && op->orr_modlist != NULL ) {
		Modifications *tmp;

		for ( ; op->orr_modlist != NULL; op->orr_modlist = tmp ) {
			tmp = op->orr_modlist->sml_next;
			ch_free( op->orr_modlist );
		}
	}

	if ( new_rdn != NULL ) {
		ldap_rdnfree_x( new_rdn, op->o_tmpmemctx );
	}
	if ( old_rdn != NULL ) {
		ldap_rdnfree_x( old_rdn, op->o_tmpmemctx );
	}

	return rs->sr_err;
}