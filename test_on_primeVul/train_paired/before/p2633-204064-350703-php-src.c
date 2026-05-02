int odbc_bindcols(odbc_result *result TSRMLS_DC)
{
	RETCODE rc;
	int i;
	SQLSMALLINT 	colnamelen; /* Not used */
	SQLLEN      	displaysize;
	SQLUSMALLINT	colfieldid;
	int		charextraalloc;

	result->values = (odbc_result_value *) safe_emalloc(sizeof(odbc_result_value), result->numcols, 0);

	result->longreadlen = ODBCG(defaultlrl);
	result->binmode = ODBCG(defaultbinmode);

	for(i = 0; i < result->numcols; i++) {
		charextraalloc = 0;
		colfieldid = SQL_COLUMN_DISPLAY_SIZE;

		rc = PHP_ODBC_SQLCOLATTRIBUTE(result->stmt, (SQLUSMALLINT)(i+1), PHP_ODBC_SQL_DESC_NAME,
				result->values[i].name, sizeof(result->values[i].name), &colnamelen, 0);
		rc = PHP_ODBC_SQLCOLATTRIBUTE(result->stmt, (SQLUSMALLINT)(i+1), SQL_COLUMN_TYPE, 
				NULL, 0, NULL, &result->values[i].coltype);
		
		/* Don't bind LONG / BINARY columns, so that fetch behaviour can
		 * be controlled by odbc_binmode() / odbc_longreadlen()
		 */
		
		switch(result->values[i].coltype) {
			case SQL_BINARY:
			case SQL_VARBINARY:
			case SQL_LONGVARBINARY:
			case SQL_LONGVARCHAR:
#if defined(ODBCVER) && (ODBCVER >= 0x0300)
			case SQL_WLONGVARCHAR:
#endif
				result->values[i].value = NULL;
				break;
				
#ifdef HAVE_ADABAS
			case SQL_TIMESTAMP:
				result->values[i].value = (char *)emalloc(27);
				SQLBindCol(result->stmt, (SQLUSMALLINT)(i+1), SQL_C_CHAR, result->values[i].value,
							27, &result->values[i].vallen);
				break;
#endif /* HAVE_ADABAS */
			case SQL_CHAR:
			case SQL_VARCHAR:
#if defined(ODBCVER) && (ODBCVER >= 0x0300)
			case SQL_WCHAR:
			case SQL_WVARCHAR:
				colfieldid = SQL_DESC_OCTET_LENGTH;
#else
				charextraalloc = 1;
#endif
			default:
				rc = PHP_ODBC_SQLCOLATTRIBUTE(result->stmt, (SQLUSMALLINT)(i+1), colfieldid,
								NULL, 0, NULL, &displaysize);
#if defined(ODBCVER) && (ODBCVER >= 0x0300)
				if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO && colfieldid == SQL_DESC_OCTET_LENGTH) {
					 /* This is  a quirk for ODBC 2.0 compatibility for broken driver implementations.
					  */
					charextraalloc = 1;
					rc = SQLColAttributes(result->stmt, (SQLUSMALLINT)(i+1), SQL_COLUMN_DISPLAY_SIZE,
								NULL, 0, NULL, &displaysize);
				}
#endif
				/* Workaround for drivers that report NVARCHAR(MAX) columns as SQL_WVARCHAR with size 0 (bug #69975) */
				if (result->values[i].coltype == SQL_WVARCHAR && displaysize == 0) {
					result->values[i].coltype = SQL_WLONGVARCHAR;
					result->values[i].value = NULL;
					break;
				}

				/* Workaround for Oracle ODBC Driver bug (#50162) when fetching TIMESTAMP column */
				if (result->values[i].coltype == SQL_TIMESTAMP) {
					displaysize += 3;
				}

				if (charextraalloc) {
					/* Since we don't know the exact # of bytes, allocate extra */
					displaysize *= 4;
				}
				result->values[i].value = (char *)emalloc(displaysize + 1);
				rc = SQLBindCol(result->stmt, (SQLUSMALLINT)(i+1), SQL_C_CHAR, result->values[i].value,
							displaysize + 1, &result->values[i].vallen);
				break;
		}
	}
	return 1;
}