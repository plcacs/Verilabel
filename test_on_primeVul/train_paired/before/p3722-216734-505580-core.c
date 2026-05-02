static void imap_parser_save_arg(struct imap_parser *parser,
				 const unsigned char *data, size_t size)
{
	struct imap_arg *arg;
	char *str;

	arg = imap_arg_create(parser);

	switch (parser->cur_type) {
	case ARG_PARSE_ATOM:
	case ARG_PARSE_TEXT:
		if (size == 3 && i_memcasecmp(data, "NIL", 3) == 0) {
			/* NIL argument. it might be an actual NIL, but if
			   we're reading astring, it's an atom and we can't
			   lose its case. */
			arg->type = IMAP_ARG_NIL;
		} else {
			/* simply save the string */
			arg->type = IMAP_ARG_ATOM;
		}
		arg->_data.str = imap_parser_strdup(parser, data, size);
		arg->str_len = size;
		break;
	case ARG_PARSE_STRING:
		/* data is quoted and may contain escapes. */
		i_assert(size > 0);

		arg->type = IMAP_ARG_STRING;
		str = p_strndup(parser->pool, data+1, size-1);

		/* remove the escapes */
		if (parser->str_first_escape >= 0 &&
		    (parser->flags & IMAP_PARSE_FLAG_NO_UNESCAPE) == 0) {
			/* -1 because we skipped the '"' prefix */
			(void)str_unescape(str + parser->str_first_escape-1);
		}
		arg->_data.str = str;
		arg->str_len = strlen(str);
		break;
	case ARG_PARSE_LITERAL_DATA:
		if ((parser->flags & IMAP_PARSE_FLAG_LITERAL_SIZE) != 0) {
			/* save literal size */
			arg->type = parser->literal_nonsync ?
				IMAP_ARG_LITERAL_SIZE_NONSYNC :
				IMAP_ARG_LITERAL_SIZE;
			arg->_data.literal_size = parser->literal_size;
			arg->literal8 = parser->literal8;
			break;
		}
		/* fall through */
	case ARG_PARSE_LITERAL_DATA_FORCED:
		if ((parser->flags & IMAP_PARSE_FLAG_LITERAL_TYPE) != 0)
			arg->type = IMAP_ARG_LITERAL;
		else
			arg->type = IMAP_ARG_STRING;
		arg->_data.str = imap_parser_strdup(parser, data, size);
		arg->literal8 = parser->literal8;
		arg->str_len = size;
		break;
	default:
                i_unreached();
	}

	parser->cur_type = ARG_PARSE_NONE;
}