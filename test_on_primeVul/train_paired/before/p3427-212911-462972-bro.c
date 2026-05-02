int ContentLine_Analyzer::DoDeliverOnce(int len, const u_char* data)
	{
	const u_char* data_start = data;

	if ( len <= 0 )
		return 0;

	for ( ; len > 0; --len, ++data )
		{
		if ( offset >= buf_len )
			InitBuffer(buf_len * 2);

		int c = data[0];

#define EMIT_LINE \
	{ \
	buf[offset] = '\0'; \
	int seq_len = data + 1 - data_start; \
	seq_delivered_in_lines = seq + seq_len; \
	last_char = c; \
	ForwardStream(offset, buf, IsOrig()); \
	offset = 0; \
	return seq_len; \
	}

		switch ( c ) {
		case '\r':
			// Look ahead for '\n'.
			if ( len > 1 && data[1] == '\n' )
				{
				--len; ++data;
				last_char = c;
				c = data[0];
				EMIT_LINE
				}

			else if ( CR_LF_as_EOL & CR_as_EOL )
				EMIT_LINE

			else
				buf[offset++] = c;
			break;

		case '\n':
			if ( last_char == '\r' )
				{
				--offset; // remove '\r'
				EMIT_LINE
				}

			else if ( CR_LF_as_EOL & LF_as_EOL )
				EMIT_LINE

			else
				{
				if ( ! suppress_weirds && Conn()->FlagEvent(SINGULAR_LF) )
					Conn()->Weird("line_terminated_with_single_LF");
				buf[offset++] = c;
				}
			break;

		case '\0':
			if ( flag_NULs )
				CheckNUL();
			else
				buf[offset++] = c;
			break;

		default:
			buf[offset++] = c;
			break;
		}

		if ( last_char == '\r' )
			if ( ! suppress_weirds && Conn()->FlagEvent(SINGULAR_CR) )
				Conn()->Weird("line_terminated_with_single_CR");

		last_char = c;
		}

	return data - data_start;
	}