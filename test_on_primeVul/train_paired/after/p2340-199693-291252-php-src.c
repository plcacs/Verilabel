SPL_METHOD(SplDoublyLinkedList, serialize)
{
	spl_dllist_object     *intern   = (spl_dllist_object*)zend_object_store_get_object(getThis() TSRMLS_CC);
	smart_str              buf      = {0};
	spl_ptr_llist_element *current  = intern->llist->head, *next;
    zval                   *flags;
	php_serialize_data_t   var_hash;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	PHP_VAR_SERIALIZE_INIT(var_hash);

	/* flags */
	MAKE_STD_ZVAL(flags);
	ZVAL_LONG(flags, intern->flags);
	php_var_serialize(&buf, &flags, &var_hash TSRMLS_CC);
	zval_ptr_dtor(&flags);

	/* elements */
	while (current) {
		smart_str_appendc(&buf, ':');
		next = current->next;

		php_var_serialize(&buf, (zval **)&current->data, &var_hash TSRMLS_CC);

		current = next;
	}

	smart_str_0(&buf);

	/* done */
	PHP_VAR_SERIALIZE_DESTROY(var_hash);

	if (buf.c) {
		RETURN_STRINGL(buf.c, buf.len, 0);
	} else {
		RETURN_NULL();
	}

} /* }}} */