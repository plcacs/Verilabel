static void managesieve_parser_save_arg(struct managesieve_parser *parser,
				 const unsigned char *data, size_t size)
{
	struct managesieve_arg *arg;
	char *str;

	arg = managesieve_arg_create(parser);

	switch (parser->cur_type) {
	case ARG_PARSE_ATOM:
		/* simply save the string */
		arg->type = MANAGESIEVE_ARG_ATOM;
		arg->_data.str = p_strndup(parser->pool, data, size);
		arg->str_len = size;
		break;
	case ARG_PARSE_STRING:
		/* data is quoted and may contain escapes. */
		if ((parser->flags & MANAGESIEVE_PARSE_FLAG_STRING_STREAM) != 0) {
			arg->type = MANAGESIEVE_ARG_STRING_STREAM;
			arg->_data.str_stream = parser->str_stream;
		} else {
			i_assert(size > 0);

			arg->type = MANAGESIEVE_ARG_STRING;
			str = p_strndup(parser->pool, data+1, size-1);

			/* remove the escapes */
			if (parser->str_first_escape >= 0 &&
				  (parser->flags & MANAGESIEVE_PARSE_FLAG_NO_UNESCAPE) == 0) {
				/* -1 because we skipped the '"' prefix */
				str_unescape(str + parser->str_first_escape-1);
			}

			arg->_data.str = str;
			arg->str_len = strlen(str);
		}
		break;
	case ARG_PARSE_LITERAL_DATA:
		if ((parser->flags & MANAGESIEVE_PARSE_FLAG_STRING_STREAM) != 0) {
			arg->type = MANAGESIEVE_ARG_STRING_STREAM;
			arg->_data.str_stream = parser->str_stream;
		} else if ((parser->flags & MANAGESIEVE_PARSE_FLAG_LITERAL_TYPE) != 0) {
			arg->type = MANAGESIEVE_ARG_LITERAL;
			arg->_data.str = p_strndup(parser->pool, data, size);
			arg->str_len = size;
		} else {
			arg->type = MANAGESIEVE_ARG_STRING;
			arg->_data.str = p_strndup(parser->pool, data, size);
			arg->str_len = size;
		}
		break;
	default:
		i_unreached();
	}

	parser->cur_type = ARG_PARSE_NONE;
}