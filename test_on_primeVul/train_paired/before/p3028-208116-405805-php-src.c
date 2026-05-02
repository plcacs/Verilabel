static xmlNodePtr master_to_xml_int(encodePtr encode, zval *data, int style, xmlNodePtr parent, int check_class_map TSRMLS_DC)
{
	xmlNodePtr node = NULL;
	int add_type = 0;

	/* Special handling of class SoapVar */
	if (data &&
	    Z_TYPE_P(data) == IS_OBJECT &&
	    Z_OBJCE_P(data) == soap_var_class_entry) {
		zval **ztype, **zdata, **zns, **zstype, **zname, **znamens;
		encodePtr enc = NULL;
		HashTable *ht = Z_OBJPROP_P(data);

		if (zend_hash_find(ht, "enc_type", sizeof("enc_type"), (void **)&ztype) == FAILURE ||
		    Z_TYPE_PP(ztype) != IS_LONG) {
			soap_error0(E_ERROR, "Encoding: SoapVar has no 'enc_type' property");
		}

		if (zend_hash_find(ht, "enc_stype", sizeof("enc_stype"), (void **)&zstype) == SUCCESS &&
		    Z_TYPE_PP(zstype) == IS_STRING) {
			if (zend_hash_find(ht, "enc_ns", sizeof("enc_ns"), (void **)&zns) == SUCCESS &&
			    Z_TYPE_PP(zns) == IS_STRING) {
				enc = get_encoder(SOAP_GLOBAL(sdl), Z_STRVAL_PP(zns), Z_STRVAL_PP(zstype));
			} else {
				zns = NULL;
				enc = get_encoder_ex(SOAP_GLOBAL(sdl), Z_STRVAL_PP(zstype), Z_STRLEN_PP(zstype));
			}
			if (enc == NULL && SOAP_GLOBAL(typemap)) {
				encodePtr *new_enc;
				smart_str nscat = {0};

				if (zns != NULL) {
					smart_str_appendl(&nscat, Z_STRVAL_PP(zns), Z_STRLEN_PP(zns));
					smart_str_appendc(&nscat, ':');
				}
				smart_str_appendl(&nscat, Z_STRVAL_PP(zstype), Z_STRLEN_PP(zstype));
				smart_str_0(&nscat);
				if (zend_hash_find(SOAP_GLOBAL(typemap), nscat.c, nscat.len + 1, (void**)&new_enc) == SUCCESS) {
					enc = *new_enc;
				}
				smart_str_free(&nscat);			
			}
		}
		if (enc == NULL) {
			enc = get_conversion(Z_LVAL_P(*ztype));
		}
		if (enc == NULL) {
			enc = encode;
		}

		if (zend_hash_find(ht, "enc_value", sizeof("enc_value"), (void **)&zdata) == FAILURE) {
			node = master_to_xml(enc, NULL, style, parent TSRMLS_CC);
		} else {
			node = master_to_xml(enc, *zdata, style, parent TSRMLS_CC);
		}

		if (style == SOAP_ENCODED || (SOAP_GLOBAL(sdl) && encode != enc)) {
			if (zend_hash_find(ht, "enc_stype", sizeof("enc_stype"), (void **)&zstype) == SUCCESS &&
			    Z_TYPE_PP(zstype) == IS_STRING) {
				if (zend_hash_find(ht, "enc_ns", sizeof("enc_ns"), (void **)&zns) == SUCCESS &&
				    Z_TYPE_PP(zns) == IS_STRING) {
					set_ns_and_type_ex(node, Z_STRVAL_PP(zns), Z_STRVAL_PP(zstype));
				} else {
					set_ns_and_type_ex(node, NULL, Z_STRVAL_PP(zstype));
				}
			}
		}

		if (zend_hash_find(ht, "enc_name", sizeof("enc_name"), (void **)&zname) == SUCCESS &&
		    Z_TYPE_PP(zname) == IS_STRING) {
			xmlNodeSetName(node, BAD_CAST(Z_STRVAL_PP(zname)));
		}
		if (zend_hash_find(ht, "enc_namens", sizeof("enc_namens"), (void **)&znamens) == SUCCESS &&
		    Z_TYPE_PP(zname) == IS_STRING) {
			xmlNsPtr nsp = encode_add_ns(node, Z_STRVAL_PP(znamens));
			xmlSetNs(node, nsp);
		}
	} else {
		if (check_class_map && SOAP_GLOBAL(class_map) && data &&
		    Z_TYPE_P(data) == IS_OBJECT &&
		    !Z_OBJPROP_P(data)->nApplyCount) {
			zend_class_entry *ce = Z_OBJCE_P(data);
			HashPosition pos;
			zval **tmp;
			char *type_name = NULL;
			uint type_len;
			ulong idx;

			for (zend_hash_internal_pointer_reset_ex(SOAP_GLOBAL(class_map), &pos);
			     zend_hash_get_current_data_ex(SOAP_GLOBAL(class_map), (void **) &tmp, &pos) == SUCCESS;
			     zend_hash_move_forward_ex(SOAP_GLOBAL(class_map), &pos)) {
				if (Z_TYPE_PP(tmp) == IS_STRING &&
				    ce->name_length == Z_STRLEN_PP(tmp) &&
				    zend_binary_strncasecmp(ce->name, ce->name_length, Z_STRVAL_PP(tmp), ce->name_length, ce->name_length) == 0 &&
				    zend_hash_get_current_key_ex(SOAP_GLOBAL(class_map), &type_name, &type_len, &idx, 0, &pos) == HASH_KEY_IS_STRING) {

					/* TODO: namespace isn't stored */
					encodePtr enc = NULL;
					if (SOAP_GLOBAL(sdl)) {
						enc = get_encoder(SOAP_GLOBAL(sdl), SOAP_GLOBAL(sdl)->target_ns, type_name);
						if (!enc) {
							enc = find_encoder_by_type_name(SOAP_GLOBAL(sdl), type_name);
						}
					}
					if (enc) {
						if (encode != enc && style == SOAP_LITERAL) {
							add_type = 1;			    			
						}
						encode = enc;
					}
					break;
				}
			}
		}

		if (encode == NULL) {
			encode = get_conversion(UNKNOWN_TYPE);
		}
		if (SOAP_GLOBAL(typemap) && encode->details.type_str) {
			smart_str nscat = {0};
			encodePtr *new_enc;

			if (encode->details.ns) {
				smart_str_appends(&nscat, encode->details.ns);
				smart_str_appendc(&nscat, ':');
			}
			smart_str_appends(&nscat, encode->details.type_str);
			smart_str_0(&nscat);
			if (zend_hash_find(SOAP_GLOBAL(typemap), nscat.c, nscat.len + 1, (void**)&new_enc) == SUCCESS) {
				encode = *new_enc;
			}
			smart_str_free(&nscat);			
		}
		if (encode->to_xml) {
			node = encode->to_xml(&encode->details, data, style, parent TSRMLS_CC);
			if (add_type) {
				set_ns_and_type(node, &encode->details);
			}
		}
	}
	return node;
}