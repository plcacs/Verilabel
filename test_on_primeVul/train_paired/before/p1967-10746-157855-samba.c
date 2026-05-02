static int samldb_check_user_account_control_acl(struct samldb_ctx *ac,
						 struct dom_sid *sid,
						 uint32_t user_account_control,
						 uint32_t user_account_control_old)
{
	int i, ret = 0;
	bool need_acl_check = false;
	struct ldb_result *res;
	const char * const sd_attrs[] = {"ntSecurityDescriptor", NULL};
	struct security_token *user_token;
	struct security_descriptor *domain_sd;
	struct ldb_dn *domain_dn = ldb_get_default_basedn(ldb_module_get_ctx(ac->module));
	const struct uac_to_guid {
		uint32_t uac;
		const char *oid;
		const char *guid;
		enum sec_privilege privilege;
		bool delete_is_privileged;
		const char *error_string;
	} map[] = {
		{
			.uac = UF_PASSWD_NOTREQD,
			.guid = GUID_DRS_UPDATE_PASSWORD_NOT_REQUIRED_BIT,
			.error_string = "Adding the UF_PASSWD_NOTREQD bit in userAccountControl requires the Update-Password-Not-Required-Bit right that was not given on the Domain object"
		},
		{
			.uac = UF_DONT_EXPIRE_PASSWD,
			.guid = GUID_DRS_UNEXPIRE_PASSWORD,
			.error_string = "Adding the UF_DONT_EXPIRE_PASSWD bit in userAccountControl requires the Unexpire-Password right that was not given on the Domain object"
		},
		{
			.uac = UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED,
			.guid = GUID_DRS_ENABLE_PER_USER_REVERSIBLY_ENCRYPTED_PASSWORD,
			.error_string = "Adding the UF_ENCRYPTED_TEXT_PASSWORD_ALLOWED bit in userAccountControl requires the Enable-Per-User-Reversibly-Encrypted-Password right that was not given on the Domain object"
		},
		{
			.uac = UF_SERVER_TRUST_ACCOUNT,
			.guid = GUID_DRS_DS_INSTALL_REPLICA,
			.error_string = "Adding the UF_SERVER_TRUST_ACCOUNT bit in userAccountControl requires the DS-Install-Replica right that was not given on the Domain object"
		},
		{
			.uac = UF_PARTIAL_SECRETS_ACCOUNT,
			.guid = GUID_DRS_DS_INSTALL_REPLICA,
			.error_string = "Adding the UF_PARTIAL_SECRETS_ACCOUNT bit in userAccountControl requires the DS-Install-Replica right that was not given on the Domain object"
		},
		{
			.uac = UF_INTERDOMAIN_TRUST_ACCOUNT,
			.oid = DSDB_CONTROL_PERMIT_INTERDOMAIN_TRUST_UAC_OID,
			.error_string = "Updating the UF_INTERDOMAIN_TRUST_ACCOUNT bit in userAccountControl is not permitted over LDAP.  This bit is restricted to the LSA CreateTrustedDomain interface",
			.delete_is_privileged = true
		},
		{
			.uac = UF_TRUSTED_FOR_DELEGATION,
			.privilege = SEC_PRIV_ENABLE_DELEGATION,
			.delete_is_privileged = true,
			.error_string = "Updating the UF_TRUSTED_FOR_DELEGATION bit in userAccountControl is not permitted without the SeEnableDelegationPrivilege"
		},
		{
			.uac = UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION,
			.privilege = SEC_PRIV_ENABLE_DELEGATION,
			.delete_is_privileged = true,
			.error_string = "Updating the UF_TRUSTED_TO_AUTHENTICATE_FOR_DELEGATION bit in userAccountControl is not permitted without the SeEnableDelegationPrivilege"
		}

	};

	if (dsdb_module_am_system(ac->module)) {
		return LDB_SUCCESS;
	}

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (user_account_control & map[i].uac) {
			need_acl_check = true;
			break;
		}
	}
	if (need_acl_check == false) {
		return LDB_SUCCESS;
	}

	user_token = acl_user_token(ac->module);
	if (user_token == NULL) {
		return LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	}

	ret = dsdb_module_search_dn(ac->module, ac, &res,
				    domain_dn,
				    sd_attrs,
				    DSDB_FLAG_NEXT_MODULE | DSDB_SEARCH_SHOW_DELETED,
				    ac->req);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	if (res->count != 1) {
		return ldb_module_operr(ac->module);
	}

	ret = dsdb_get_sd_from_ldb_message(ldb_module_get_ctx(ac->module),
					   ac, res->msgs[0], &domain_sd);

	if (ret != LDB_SUCCESS) {
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		uint32_t this_uac_new = user_account_control & map[i].uac;
		uint32_t this_uac_old = user_account_control_old & map[i].uac;
		if (this_uac_new != this_uac_old) {
			if (this_uac_old != 0) {
				if (map[i].delete_is_privileged == false) {
					continue;
				}
			}
			if (map[i].oid) {
				struct ldb_control *control = ldb_request_get_control(ac->req, map[i].oid);
				if (control == NULL) {
					ret = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
				}
			} else if (map[i].privilege != SEC_PRIV_INVALID) {
				bool have_priv = security_token_has_privilege(user_token,
									      map[i].privilege);
				if (have_priv == false) {
					ret = LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
				}
			} else {
				ret = acl_check_extended_right(ac, domain_sd,
							       user_token,
							       map[i].guid,
							       SEC_ADS_CONTROL_ACCESS,
							       sid);
			}
			if (ret != LDB_SUCCESS) {
				break;
			}
		}
	}
	if (ret == LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS) {
		switch (ac->req->operation) {
		case LDB_ADD:
			ldb_asprintf_errstring(ldb_module_get_ctx(ac->module),
					       "Failed to add %s: %s",
					       ldb_dn_get_linearized(ac->msg->dn),
					       map[i].error_string);
			break;
		case LDB_MODIFY:
			ldb_asprintf_errstring(ldb_module_get_ctx(ac->module),
					       "Failed to modify %s: %s",
					       ldb_dn_get_linearized(ac->msg->dn),
					       map[i].error_string);
			break;
		default:
			return ldb_module_operr(ac->module);
		}
		if (map[i].guid) {
			dsdb_acl_debug(domain_sd, acl_user_token(ac->module),
				       domain_dn,
				       true,
				       10);
		}
	}
	return ret;
}