PHP_FUNCTION(odbc_execute)
{ 
	zval *pv_res, *pv_param_arr, **tmp;
	typedef struct params_t {
		SQLLEN vallen;
		int fp;
	} params_t;
	params_t *params = NULL;
	char *filename;
	unsigned char otype;
   	SQLSMALLINT sqltype, ctype, scale;
	SQLSMALLINT nullable;
	SQLULEN precision;
   	odbc_result *result;
	int numArgs, i, ne;
	RETCODE rc;
	
	numArgs = ZEND_NUM_ARGS();
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|a", &pv_res, &pv_param_arr) == FAILURE) {
		return;
	}

	ZEND_FETCH_RESOURCE(result, odbc_result *, &pv_res, -1, "ODBC result", le_result);
	
	/* XXX check for already bound parameters*/
	if (result->numparams > 0 && numArgs == 1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "No parameters to SQL statement given");
		RETURN_FALSE;
	}

	if (result->numparams > 0) {
		if ((ne = zend_hash_num_elements(Z_ARRVAL_P(pv_param_arr))) < result->numparams) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING,"Not enough parameters (%d should be %d) given", ne, result->numparams);
			RETURN_FALSE;
		}

		zend_hash_internal_pointer_reset(Z_ARRVAL_P(pv_param_arr));
		params = (params_t *)safe_emalloc(sizeof(params_t), result->numparams, 0);
		for(i = 0; i < result->numparams; i++) {
			params[i].fp = -1;
		}
		
		for(i = 1; i <= result->numparams; i++) {
			if (zend_hash_get_current_data(Z_ARRVAL_P(pv_param_arr), (void **) &tmp) == FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,"Error getting parameter");
				SQLFreeStmt(result->stmt,SQL_RESET_PARAMS);
				for (i = 0; i < result->numparams; i++) {
					if (params[i].fp != -1) {
						close(params[i].fp);
					}
				}
				efree(params);
				RETURN_FALSE;
			}

			otype = (*tmp)->type;
			convert_to_string_ex(tmp);
			if (Z_TYPE_PP(tmp) != IS_STRING) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING,"Error converting parameter");
				SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
				for (i = 0; i < result->numparams; i++) {
					if (params[i].fp != -1) {
						close(params[i].fp);
					}
				}
				efree(params);
				RETURN_FALSE;
			}
			
			rc = SQLDescribeParam(result->stmt, (SQLUSMALLINT)i, &sqltype, &precision, &scale, &nullable);
			params[i-1].vallen = Z_STRLEN_PP(tmp);
			params[i-1].fp = -1;
			if (rc == SQL_ERROR) {
				odbc_sql_error(result->conn_ptr, result->stmt, "SQLDescribeParameter");	
				SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
				for (i = 0; i < result->numparams; i++) {
					if (params[i].fp != -1) {
						close(params[i].fp);
					}
				}
				efree(params);
				RETURN_FALSE;
			}

			if (IS_SQL_BINARY(sqltype)) {
				ctype = SQL_C_BINARY;
			} else {
				ctype = SQL_C_CHAR;
			}

			if (Z_STRLEN_PP(tmp) > 2 &&
				Z_STRVAL_PP(tmp)[0] == '\'' &&
				Z_STRVAL_PP(tmp)[Z_STRLEN_PP(tmp) - 1] == '\'') {
				if (strlen(tmp) != Z_STRLEN_PP(tmp)) {
					RETURN_FALSE;
				}

				filename = estrndup(&Z_STRVAL_PP(tmp)[1], Z_STRLEN_PP(tmp) - 2);

				/* Check for safe mode. */
				if (PG(safe_mode) && (!php_checkuid(filename, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
					efree(filename);
					efree(params);
					RETURN_FALSE;
				}

				/* Check the basedir */
				if (php_check_open_basedir(filename TSRMLS_CC)) {
					efree(filename);
					SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
					for (i = 0; i < result->numparams; i++) {
						if (params[i].fp != -1) {
							close(params[i].fp);
						}
					}
					efree(params);
					RETURN_FALSE;
				}

				if ((params[i-1].fp = open(filename,O_RDONLY)) == -1) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING,"Can't open file %s", filename);
					SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
					for (i = 0; i < result->numparams; i++) {
						if (params[i].fp != -1) {
							close(params[i].fp);
						}
					}
					efree(params);
					efree(filename);
					RETURN_FALSE;
				}

				efree(filename);

				params[i-1].vallen = SQL_LEN_DATA_AT_EXEC(0);

				rc = SQLBindParameter(result->stmt, (SQLUSMALLINT)i, SQL_PARAM_INPUT,
									  ctype, sqltype, precision, scale,
									  (void *)params[i-1].fp, 0,
									  &params[i-1].vallen);
			} else {
#ifdef HAVE_DBMAKER
				precision = params[i-1].vallen;
#endif
				if (otype == IS_NULL) {
					params[i-1].vallen = SQL_NULL_DATA;
				}

				rc = SQLBindParameter(result->stmt, (SQLUSMALLINT)i, SQL_PARAM_INPUT,
									  ctype, sqltype, precision, scale,
									  Z_STRVAL_PP(tmp), 0,
									  &params[i-1].vallen);
			}
			if (rc == SQL_ERROR) {
				odbc_sql_error(result->conn_ptr, result->stmt, "SQLBindParameter");	
				SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
				for (i = 0; i < result->numparams; i++) {
					if (params[i].fp != -1) {
						close(params[i].fp);
					}
				}
				efree(params);
				RETURN_FALSE;
			}
			zend_hash_move_forward(Z_ARRVAL_P(pv_param_arr));
		}
	}
	/* Close cursor, needed for doing multiple selects */
	rc = SQLFreeStmt(result->stmt, SQL_CLOSE);

	if (rc == SQL_ERROR) {
		odbc_sql_error(result->conn_ptr, result->stmt, "SQLFreeStmt");	
	}

	rc = SQLExecute(result->stmt);

	result->fetched = 0;
	if (rc == SQL_NEED_DATA) {
		char buf[4096];
		int fp, nbytes;
		while (rc == SQL_NEED_DATA) {
			rc = SQLParamData(result->stmt, (void*)&fp);
			if (rc == SQL_NEED_DATA) {
				while ((nbytes = read(fp, &buf, 4096)) > 0) {
					SQLPutData(result->stmt, (void*)&buf, nbytes);
				}
			}
		}
	} else {
		switch (rc) {
			case SQL_SUCCESS:
				break;
			case SQL_NO_DATA_FOUND:
			case SQL_SUCCESS_WITH_INFO:
				odbc_sql_error(result->conn_ptr, result->stmt, "SQLExecute");
				break;
			default:
				odbc_sql_error(result->conn_ptr, result->stmt, "SQLExecute");
				RETVAL_FALSE;
		}
	}	
	
	if (result->numparams > 0) {
		SQLFreeStmt(result->stmt, SQL_RESET_PARAMS);
		for(i = 0; i < result->numparams; i++) {
			if (params[i].fp != -1) {
				close(params[i].fp);
			}
		}
		efree(params);
	}

	if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO || rc == SQL_NO_DATA_FOUND) {
		RETVAL_TRUE;
	}

	if (result->numcols == 0) {
		SQLNumResultCols(result->stmt, &(result->numcols));

		if (result->numcols > 0) {
			if (!odbc_bindcols(result TSRMLS_CC)) {
				efree(result);
				RETVAL_FALSE;
			}
		} else {
			result->values = NULL;
		}
	}
}