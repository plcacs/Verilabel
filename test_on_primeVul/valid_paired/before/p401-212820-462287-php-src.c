PHP_FUNCTION( msgfmt_parse_message )
{
	UChar      *spattern = NULL;
	int         spattern_len = 0;
	char       *pattern = NULL;
	size_t      pattern_len = 0;
	const char *slocale = NULL;
	size_t      slocale_len = 0;
	char       *source = NULL;
	size_t      src_len = 0;
	MessageFormatter_object mf;
	MessageFormatter_object *mfo = &mf;

	/* Parse parameters. */
	if( zend_parse_parameters( ZEND_NUM_ARGS(), "sss",
		  &slocale, &slocale_len, &pattern, &pattern_len, &source, &src_len ) == FAILURE )
	{
		intl_error_set( NULL, U_ILLEGAL_ARGUMENT_ERROR,
			"msgfmt_parse_message: unable to parse input params", 0 );

		RETURN_FALSE;
	}

	memset(mfo, 0, sizeof(*mfo));
	msgformat_data_init(&mfo->mf_data);

	if(pattern && pattern_len) {
		intl_convert_utf8_to_utf16(&spattern, &spattern_len, pattern, pattern_len, &INTL_DATA_ERROR_CODE(mfo));
		if( U_FAILURE(INTL_DATA_ERROR_CODE((mfo))) )
		{
			intl_error_set( NULL, U_ILLEGAL_ARGUMENT_ERROR,
				"msgfmt_parse_message: error converting pattern to UTF-16", 0 );
			RETURN_FALSE;
		}
	} else {
		spattern_len = 0;
		spattern = NULL;
	}

	if(slocale_len == 0) {
		slocale = intl_locale_get_default();
	}

#ifdef MSG_FORMAT_QUOTE_APOS
	if(msgformat_fix_quotes(&spattern, &spattern_len, &INTL_DATA_ERROR_CODE(mfo)) != SUCCESS) {
		intl_error_set( NULL, U_INVALID_FORMAT_ERROR,
			"msgfmt_parse_message: error converting pattern to quote-friendly format", 0 );
		RETURN_FALSE;
	}
#endif

	/* Create an ICU message formatter. */
	MSG_FORMAT_OBJECT(mfo) = umsg_open(spattern, spattern_len, slocale, NULL, &INTL_DATA_ERROR_CODE(mfo));
	if(spattern && spattern_len) {
		efree(spattern);
	}
	INTL_METHOD_CHECK_STATUS(mfo, "Creating message formatter failed");

	msgfmt_do_parse(mfo, source, src_len, return_value);

	/* drop the temporary formatter */
	msgformat_data_free(&mfo->mf_data);
}