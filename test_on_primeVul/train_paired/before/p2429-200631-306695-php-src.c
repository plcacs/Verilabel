static int phar_build(zend_object_iterator *iter, void *puser) /* {{{ */
{
	zval *value;
	zend_bool close_fp = 1;
	struct _phar_t *p_obj = (struct _phar_t*) puser;
	uint32_t str_key_len, base_len = p_obj->l;
	phar_entry_data *data;
	php_stream *fp;
	size_t fname_len;
	size_t contents_len;
	char *fname, *error = NULL, *base = p_obj->b, *save = NULL, *temp = NULL;
	zend_string *opened;
	char *str_key;
	zend_class_entry *ce = p_obj->c;
	phar_archive_object *phar_obj = p_obj->p;

	value = iter->funcs->get_current_data(iter);

	if (EG(exception)) {
		return ZEND_HASH_APPLY_STOP;
	}

	if (!value) {
		/* failure in get_current_data */
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned no value", ZSTR_VAL(ce->name));
		return ZEND_HASH_APPLY_STOP;
	}

	switch (Z_TYPE_P(value)) {
		case IS_STRING:
			break;
		case IS_RESOURCE:
			php_stream_from_zval_no_verify(fp, value);

			if (!fp) {
				zend_throw_exception_ex(spl_ce_BadMethodCallException, 0, "Iterator %s returned an invalid stream handle", ZSTR_VAL(ce->name));
				return ZEND_HASH_APPLY_STOP;
			}

			if (iter->funcs->get_current_key) {
				zval key;
				iter->funcs->get_current_key(iter, &key);

				if (EG(exception)) {
					return ZEND_HASH_APPLY_STOP;
				}

				if (Z_TYPE(key) != IS_STRING) {
					zval_dtor(&key);
					zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned an invalid key (must return a string)", ZSTR_VAL(ce->name));
					return ZEND_HASH_APPLY_STOP;
				}

				if (ZEND_SIZE_T_INT_OVFL(Z_STRLEN(key))) {
					zval_dtor(&key);
					zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned an invalid key (too long)", ZSTR_VAL(ce->name));
					return ZEND_HASH_APPLY_STOP;
				}

				str_key_len = (int)Z_STRLEN(key);
				str_key = estrndup(Z_STRVAL(key), str_key_len);

				save = str_key;
				zval_dtor(&key);
			} else {
				zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned an invalid key (must return a string)", ZSTR_VAL(ce->name));
				return ZEND_HASH_APPLY_STOP;
			}

			close_fp = 0;
			opened = zend_string_init("[stream]", sizeof("[stream]") - 1, 0);
			goto after_open_fp;
		case IS_OBJECT:
			if (instanceof_function(Z_OBJCE_P(value), spl_ce_SplFileInfo)) {
				char *test = NULL;
				zval dummy;
				spl_filesystem_object *intern = (spl_filesystem_object*)((char*)Z_OBJ_P(value) - Z_OBJ_P(value)->handlers->offset);

				if (!base_len) {
					zend_throw_exception_ex(spl_ce_BadMethodCallException, 0, "Iterator %s returns an SplFileInfo object, so base directory must be specified", ZSTR_VAL(ce->name));
					return ZEND_HASH_APPLY_STOP;
				}

				switch (intern->type) {
					case SPL_FS_DIR:
						test = spl_filesystem_object_get_path(intern, NULL);
						fname_len = spprintf(&fname, 0, "%s%c%s", test, DEFAULT_SLASH, intern->u.dir.entry.d_name);
						php_stat(fname, fname_len, FS_IS_DIR, &dummy);

						if (Z_TYPE(dummy) == IS_TRUE) {
							/* ignore directories */
							efree(fname);
							return ZEND_HASH_APPLY_KEEP;
						}

						test = expand_filepath(fname, NULL);
						efree(fname);

						if (test) {
							fname = test;
							fname_len = strlen(fname);
						} else {
							zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Could not resolve file path");
							return ZEND_HASH_APPLY_STOP;
						}

						save = fname;
						goto phar_spl_fileinfo;
					case SPL_FS_INFO:
					case SPL_FS_FILE:
						fname = expand_filepath(intern->file_name, NULL);
						if (!fname) {
							zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Could not resolve file path");
							return ZEND_HASH_APPLY_STOP;
						}

						fname_len = strlen(fname);
						save = fname;
						goto phar_spl_fileinfo;
				}
			}
			/* fall-through */
		default:
			zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned an invalid value (must return a string)", ZSTR_VAL(ce->name));
			return ZEND_HASH_APPLY_STOP;
	}

	fname = Z_STRVAL_P(value);
	fname_len = Z_STRLEN_P(value);

phar_spl_fileinfo:
	if (base_len) {
		temp = expand_filepath(base, NULL);
		if (!temp) {
			zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Could not resolve file path");
			if (save) {
				efree(save);
			}
			return ZEND_HASH_APPLY_STOP;
		}

		base = temp;
		base_len = (int)strlen(base);

		if (strstr(fname, base)) {
			str_key_len = fname_len - base_len;

			if (str_key_len <= 0) {
				if (save) {
					efree(save);
					efree(temp);
				}
				return ZEND_HASH_APPLY_KEEP;
			}

			str_key = fname + base_len;

			if (*str_key == '/' || *str_key == '\\') {
				str_key++;
				str_key_len--;
			}

		} else {
			zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned a path \"%s\" that is not in the base directory \"%s\"", ZSTR_VAL(ce->name), fname, base);

			if (save) {
				efree(save);
				efree(temp);
			}

			return ZEND_HASH_APPLY_STOP;
		}
	} else {
		if (iter->funcs->get_current_key) {
			zval key;
			iter->funcs->get_current_key(iter, &key);

			if (EG(exception)) {
				return ZEND_HASH_APPLY_STOP;
			}

			if (Z_TYPE(key) != IS_STRING) {
				zval_dtor(&key);
				zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned an invalid key (must return a string)", ZSTR_VAL(ce->name));
				return ZEND_HASH_APPLY_STOP;
			}

			if (ZEND_SIZE_T_INT_OVFL(Z_STRLEN(key))) {
				zval_dtor(&key);
				zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned an invalid key (too long)", ZSTR_VAL(ce->name));
				return ZEND_HASH_APPLY_STOP;
			}

			str_key_len = (int)Z_STRLEN(key);
			str_key = estrndup(Z_STRVAL(key), str_key_len);

			save = str_key;
			zval_dtor(&key);
		} else {
			zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned an invalid key (must return a string)", ZSTR_VAL(ce->name));
			return ZEND_HASH_APPLY_STOP;
		}
	}

	if (php_check_open_basedir(fname)) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned a path \"%s\" that open_basedir prevents opening", ZSTR_VAL(ce->name), fname);

		if (save) {
			efree(save);
		}

		if (temp) {
			efree(temp);
		}

		return ZEND_HASH_APPLY_STOP;
	}

	/* try to open source file, then create internal phar file and copy contents */
	fp = php_stream_open_wrapper(fname, "rb", STREAM_MUST_SEEK|0, &opened);

	if (!fp) {
		zend_throw_exception_ex(spl_ce_UnexpectedValueException, 0, "Iterator %s returned a file that could not be opened \"%s\"", ZSTR_VAL(ce->name), fname);

		if (save) {
			efree(save);
		}

		if (temp) {
			efree(temp);
		}

		return ZEND_HASH_APPLY_STOP;
	}
after_open_fp:
	if (str_key_len >= sizeof(".phar")-1 && !memcmp(str_key, ".phar", sizeof(".phar")-1)) {
		/* silently skip any files that would be added to the magic .phar directory */
		if (save) {
			efree(save);
		}

		if (temp) {
			efree(temp);
		}

		if (opened) {
			zend_string_release(opened);
		}

		if (close_fp) {
			php_stream_close(fp);
		}

		return ZEND_HASH_APPLY_KEEP;
	}

	if (!(data = phar_get_or_create_entry_data(phar_obj->archive->fname, phar_obj->archive->fname_len, str_key, str_key_len, "w+b", 0, &error, 1))) {
		zend_throw_exception_ex(spl_ce_BadMethodCallException, 0, "Entry %s cannot be created: %s", str_key, error);
		efree(error);

		if (save) {
			efree(save);
		}

		if (opened) {
			zend_string_release(opened);
		}

		if (temp) {
			efree(temp);
		}

		if (close_fp) {
			php_stream_close(fp);
		}

		return ZEND_HASH_APPLY_STOP;

	} else {
		if (error) {
			efree(error);
		}
		/* convert to PHAR_UFP */
		if (data->internal_file->fp_type == PHAR_MOD) {
			php_stream_close(data->internal_file->fp);
		}

		data->internal_file->fp = NULL;
		data->internal_file->fp_type = PHAR_UFP;
		data->internal_file->offset_abs = data->internal_file->offset = php_stream_tell(p_obj->fp);
		data->fp = NULL;
		php_stream_copy_to_stream_ex(fp, p_obj->fp, PHP_STREAM_COPY_ALL, &contents_len);
		data->internal_file->uncompressed_filesize = data->internal_file->compressed_filesize =
			php_stream_tell(p_obj->fp) - data->internal_file->offset;
	}

	if (close_fp) {
		php_stream_close(fp);
	}

	add_assoc_str(p_obj->ret, str_key, opened);

	if (save) {
		efree(save);
	}

	if (temp) {
		efree(temp);
	}

	data->internal_file->compressed_filesize = data->internal_file->uncompressed_filesize = contents_len;
	phar_entry_delref(data);

	return ZEND_HASH_APPLY_KEEP;
}