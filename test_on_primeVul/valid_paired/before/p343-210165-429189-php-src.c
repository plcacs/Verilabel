PHP_FUNCTION(enchant_broker_request_dict)
{
	zval *broker;
	enchant_broker *pbroker;
	enchant_dict *dict;
	EnchantDict *d;
	char *tag;
	int taglen;
	int pos;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &broker, &tag, &taglen) == FAILURE) {
		RETURN_FALSE;
	}

	PHP_ENCHANT_GET_BROKER;
	
	if (taglen == 0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Tag cannot be empty");
		RETURN_FALSE;
	}

	d = enchant_broker_request_dict(pbroker->pbroker, (const char *)tag);
	if (d) {
		if (pbroker->dictcnt) {
			pbroker->dict = (enchant_dict **)erealloc(pbroker->dict, sizeof(enchant_dict *) * pbroker->dictcnt);
			pos = pbroker->dictcnt++;
		} else {
			pbroker->dict = (enchant_dict **)emalloc(sizeof(enchant_dict *));
			pos = 0;
			pbroker->dictcnt++;
		}

		dict = pbroker->dict[pos] = (enchant_dict *)emalloc(sizeof(enchant_dict));
		dict->id = pos;
		dict->pbroker = pbroker;
		dict->pdict = d;
		dict->prev = pos ? pbroker->dict[pos-1] : NULL;
		dict->next = NULL;
		pbroker->dict[pos] = dict;

		if (pos) {
			pbroker->dict[pos-1]->next = dict;
		}

		dict->rsrc_id = ZEND_REGISTER_RESOURCE(return_value, dict, le_enchant_dict);
		zend_list_addref(pbroker->rsrc_id);
	} else {
		RETURN_FALSE;
	}
}