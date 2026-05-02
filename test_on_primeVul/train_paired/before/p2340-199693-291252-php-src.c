SPL_METHOD(SplDoublyLinkedList, unserialize)
{
	spl_dllist_object     *intern   = (spl_dllist_object*)zend_object_store_get_object(getThis() TSRMLS_CC);
	zval                  *flags, *elem;
	char *buf;
	int buf_len;
	const unsigned char *p, *s;
	php_unserialize_data_t var_hash;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &buf, &buf_len) == FAILURE) {
		return;
	}

	if (buf_len == 0) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Serialized string cannot be empty");
		return;
	}

	s = p = (const unsigned char*)buf;
	PHP_VAR_UNSERIALIZE_INIT(var_hash);

	/* flags */
	ALLOC_INIT_ZVAL(flags);
	if (!php_var_unserialize(&flags, &p, s + buf_len, &var_hash TSRMLS_CC) || Z_TYPE_P(flags) != IS_LONG) {
		zval_ptr_dtor(&flags);
		goto error;
	}
	intern->flags = Z_LVAL_P(flags);
	zval_ptr_dtor(&flags);

	/* elements */
	while(*p == ':') {
		++p;
		ALLOC_INIT_ZVAL(elem);
		if (!php_var_unserialize(&elem, &p, s + buf_len, &var_hash TSRMLS_CC)) {
			zval_ptr_dtor(&elem);
			goto error;
		}

		spl_ptr_llist_push(intern->llist, elem TSRMLS_CC);
	}

	if (*p != '\0') {
		goto error;
	}

	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
	return;

error:
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
	zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0 TSRMLS_CC, "Error at offset %ld of %d bytes", (long)((char*)p - buf), buf_len);
	return;

} /* }}} */