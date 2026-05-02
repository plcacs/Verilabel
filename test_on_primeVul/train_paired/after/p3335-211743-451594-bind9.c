configure_zone_ssutable(const cfg_obj_t *zconfig, dns_zone_t *zone,
			const char *zname)
{
	const cfg_obj_t *updatepolicy = NULL;
	const cfg_listelt_t *element, *element2;
	dns_ssutable_t *table = NULL;
	isc_mem_t *mctx = dns_zone_getmctx(zone);
	bool autoddns = false;
	isc_result_t result;

	(void)cfg_map_get(zconfig, "update-policy", &updatepolicy);

	if (updatepolicy == NULL) {
		dns_zone_setssutable(zone, NULL);
		return (ISC_R_SUCCESS);
	}

	if (cfg_obj_isstring(updatepolicy) &&
	    strcmp("local", cfg_obj_asstring(updatepolicy)) == 0) {
		autoddns = true;
		updatepolicy = NULL;
	}

	result = dns_ssutable_create(mctx, &table);
	if (result != ISC_R_SUCCESS)
		return (result);

	for (element = cfg_list_first(updatepolicy);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		const cfg_obj_t *stmt = cfg_listelt_value(element);
		const cfg_obj_t *mode = cfg_tuple_get(stmt, "mode");
		const cfg_obj_t *identity = cfg_tuple_get(stmt, "identity");
		const cfg_obj_t *matchtype = cfg_tuple_get(stmt, "matchtype");
		const cfg_obj_t *dname = cfg_tuple_get(stmt, "name");
		const cfg_obj_t *typelist = cfg_tuple_get(stmt, "types");
		const char *str;
		bool grant = false;
		bool usezone = false;
		dns_ssumatchtype_t mtype = DNS_SSUMATCHTYPE_NAME;
		dns_fixedname_t fname, fident;
		isc_buffer_t b;
		dns_rdatatype_t *types;
		unsigned int i, n;

		str = cfg_obj_asstring(mode);
		if (strcasecmp(str, "grant") == 0) {
			grant = true;
		} else if (strcasecmp(str, "deny") == 0) {
			grant = false;
		} else {
			INSIST(0);
			ISC_UNREACHABLE();
		}

		str = cfg_obj_asstring(matchtype);
		CHECK(dns_ssu_mtypefromstring(str, &mtype));
		if (mtype == dns_ssumatchtype_subdomain &&
		    strcasecmp(str, "zonesub") == 0) {
			usezone = true;
		}

		dns_fixedname_init(&fident);
		str = cfg_obj_asstring(identity);
		isc_buffer_constinit(&b, str, strlen(str));
		isc_buffer_add(&b, strlen(str));
		result = dns_name_fromtext(dns_fixedname_name(&fident), &b,
					   dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
				    "'%s' is not a valid name", str);
			goto cleanup;
		}

		dns_fixedname_init(&fname);
		if (usezone) {
			result = dns_name_copy(dns_zone_getorigin(zone),
					       dns_fixedname_name(&fname),
					       NULL);
			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
					    "error copying origin: %s",
					    isc_result_totext(result));
				goto cleanup;
			}
		} else {
			str = cfg_obj_asstring(dname);
			isc_buffer_constinit(&b, str, strlen(str));
			isc_buffer_add(&b, strlen(str));
			result = dns_name_fromtext(dns_fixedname_name(&fname),
						   &b, dns_rootname, 0, NULL);
			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
					    "'%s' is not a valid name", str);
				goto cleanup;
			}
		}

		n = ns_config_listcount(typelist);
		if (n == 0)
			types = NULL;
		else {
			types = isc_mem_get(mctx, n * sizeof(dns_rdatatype_t));
			if (types == NULL) {
				result = ISC_R_NOMEMORY;
				goto cleanup;
			}
		}

		i = 0;
		for (element2 = cfg_list_first(typelist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2))
		{
			const cfg_obj_t *typeobj;
			isc_textregion_t r;

			INSIST(i < n);

			typeobj = cfg_listelt_value(element2);
			str = cfg_obj_asstring(typeobj);
			DE_CONST(str, r.base);
			r.length = strlen(str);

			result = dns_rdatatype_fromtext(&types[i++], &r);
			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(identity, ns_g_lctx, ISC_LOG_ERROR,
					    "'%s' is not a valid type", str);
				isc_mem_put(mctx, types,
					    n * sizeof(dns_rdatatype_t));
				goto cleanup;
			}
		}
		INSIST(i == n);

		result = dns_ssutable_addrule(table, grant,
					      dns_fixedname_name(&fident),
					      mtype,
					      dns_fixedname_name(&fname),
					      n, types);
		if (types != NULL)
			isc_mem_put(mctx, types, n * sizeof(dns_rdatatype_t));
		if (result != ISC_R_SUCCESS) {
			goto cleanup;
		}
	}

	/*
	 * If "update-policy local;" and a session key exists,
	 * then use the default policy, which is equivalent to:
	 * update-policy { grant <session-keyname> zonesub any; };
	 */
	if (autoddns) {
		dns_rdatatype_t any = dns_rdatatype_any;

		if (ns_g_server->session_keyname == NULL) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "failed to enable auto DDNS policy "
				      "for zone %s: session key not found",
				      zname);
			result = ISC_R_NOTFOUND;
			goto cleanup;
		}

		result = dns_ssutable_addrule(table, true,
					      ns_g_server->session_keyname,
					      DNS_SSUMATCHTYPE_LOCAL,
					      dns_zone_getorigin(zone),
					      1, &any);

		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	result = ISC_R_SUCCESS;
	dns_zone_setssutable(zone, table);

 cleanup:
	dns_ssutable_detach(&table);
	return (result);
}