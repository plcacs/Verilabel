PHP_FUNCTION(ldap_get_entries)
{
	zval *link, *result;
	LDAPMessage *ldap_result, *ldap_result_entry;
	zval *tmp1, *tmp2;
	ldap_linkdata *ld;
	LDAP *ldap;
	int num_entries, num_attrib, num_values, i;
	BerElement *ber;
	char *attribute;
	size_t attr_len;
	struct berval **ldap_value;
	char *dn;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rr", &link, &result) != SUCCESS) {
		return;
	}

	ZEND_FETCH_RESOURCE(ld, ldap_linkdata *, &link, -1, "ldap link", le_link);
	ZEND_FETCH_RESOURCE(ldap_result, LDAPMessage *, &result, -1, "ldap result", le_result);

	ldap = ld->link;
	num_entries = ldap_count_entries(ldap, ldap_result);

	array_init(return_value);
	add_assoc_long(return_value, "count", num_entries);

	if (num_entries == 0) {
		return;
	}

	ldap_result_entry = ldap_first_entry(ldap, ldap_result);
	if (ldap_result_entry == NULL) {
		zval_dtor(return_value);
		RETURN_FALSE;
	}

	num_entries = 0;
	while (ldap_result_entry != NULL) {
		MAKE_STD_ZVAL(tmp1);
		array_init(tmp1);

		num_attrib = 0;
		attribute = ldap_first_attribute(ldap, ldap_result_entry, &ber);

		while (attribute != NULL) {
			ldap_value = ldap_get_values_len(ldap, ldap_result_entry, attribute);
			num_values = ldap_count_values_len(ldap_value);

			MAKE_STD_ZVAL(tmp2);
			array_init(tmp2);
			add_assoc_long(tmp2, "count", num_values);
			for (i = 0; i < num_values; i++) {
				add_index_stringl(tmp2, i, ldap_value[i]->bv_val, ldap_value[i]->bv_len, 1);
			}
			ldap_value_free_len(ldap_value);

			attr_len = strlen(attribute);
			zend_hash_update(Z_ARRVAL_P(tmp1), php_strtolower(attribute, attr_len), attr_len+1, (void *) &tmp2, sizeof(zval *), NULL);
			add_index_string(tmp1, num_attrib, attribute, 1);

			num_attrib++;
#if (LDAP_API_VERSION > 2000) || HAVE_NSLDAP || HAVE_ORALDAP || WINDOWS
			ldap_memfree(attribute);
#endif
			attribute = ldap_next_attribute(ldap, ldap_result_entry, ber);
		}
#if (LDAP_API_VERSION > 2000) || HAVE_NSLDAP || HAVE_ORALDAP || WINDOWS
		if (ber != NULL) {
			ber_free(ber, 0);
		}
#endif

		add_assoc_long(tmp1, "count", num_attrib);
		dn = ldap_get_dn(ldap, ldap_result_entry);
		add_assoc_string(tmp1, "dn", dn, 1);
#if (LDAP_API_VERSION > 2000) || HAVE_NSLDAP || HAVE_ORALDAP || WINDOWS
		ldap_memfree(dn);
#else
		free(dn);
#endif

		zend_hash_index_update(Z_ARRVAL_P(return_value), num_entries, (void *) &tmp1, sizeof(zval *), NULL);

		num_entries++;
		ldap_result_entry = ldap_next_entry(ldap, ldap_result_entry);
	}

	add_assoc_long(return_value, "count", num_entries);

}