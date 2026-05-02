	int bdecode(char const* start, char const* end, bdecode_node& ret
		, error_code& ec, int* error_pos, int depth_limit, int token_limit)
	{
		ec.clear();
		ret.clear();

		if (end - start > bdecode_token::max_offset)
		{
			if (error_pos) *error_pos = 0;
			ec = bdecode_errors::limit_exceeded;
			return -1;
		}

		// this is the stack of bdecode_token indices, into m_tokens.
		// sp is the stack pointer, as index into the array, stack
		int sp = 0;
		stack_frame* stack = TORRENT_ALLOCA(stack_frame, depth_limit);

		char const* const orig_start = start;

		if (start == end)
			TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);

		while (start <= end)
		{
			if (start >= end) TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);

			if (sp >= depth_limit)
				TORRENT_FAIL_BDECODE(bdecode_errors::depth_exceeded);

			--token_limit;
			if (token_limit < 0)
				TORRENT_FAIL_BDECODE(bdecode_errors::limit_exceeded);

			// look for a new token
			const char t = *start;

			const int current_frame = sp;

			// if we're currently parsing a dictionary, assert that
			// every other node is a string.
			if (current_frame > 0
				&& ret.m_tokens[stack[current_frame-1].token].type == bdecode_token::dict)
			{
				if (stack[current_frame-1].state == 0)
				{
					// the current parent is a dict and we are parsing a key.
					// only allow a digit (for a string) or 'e' to terminate
					if (!numeric(t) && t != 'e')
						TORRENT_FAIL_BDECODE(bdecode_errors::expected_digit);
				}
			}

			switch (t)
			{
				case 'd':
					stack[sp++] = ret.m_tokens.size();
					// we push it into the stack so that we know where to fill
					// in the next_node field once we pop this node off the stack.
					// i.e. get to the node following the dictionary in the buffer
					ret.m_tokens.push_back(bdecode_token(start - orig_start
						, bdecode_token::dict));
					++start;
					break;
				case 'l':
					stack[sp++] = ret.m_tokens.size();
					// we push it into the stack so that we know where to fill
					// in the next_node field once we pop this node off the stack.
					// i.e. get to the node following the list in the buffer
					ret.m_tokens.push_back(bdecode_token(start - orig_start
						, bdecode_token::list));
					++start;
					break;
				case 'i':
				{
					char const* int_start = start;
					bdecode_errors::error_code_enum e = bdecode_errors::no_error;
					// +1 here to point to the first digit, rather than 'i'
					start = check_integer(start + 1, end, e);
					if (e)
					{
						// in order to gracefully terminate the tree,
						// make sure the end of the previous token is set correctly
						if (error_pos) *error_pos = start - orig_start;
						error_pos = NULL;
						start = int_start;
						TORRENT_FAIL_BDECODE(e);
					}
					ret.m_tokens.push_back(bdecode_token(int_start - orig_start
						, 1, bdecode_token::integer, 1));
					TORRENT_ASSERT(*start == 'e');

					// skip 'e'
					++start;
					break;
				}
				case 'e':
				{
					// this is the end of a list or dict
					if (sp == 0)
						TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);

					if (sp > 0
						&& ret.m_tokens[stack[sp-1].token].type == bdecode_token::dict
						&& stack[sp-1].state == 1)
					{
						// this means we're parsing a dictionary and about to parse a
						// value associated with a key. Instead, we got a termination
						TORRENT_FAIL_BDECODE(bdecode_errors::expected_value);
					}

					// insert the end-of-sequence token
					ret.m_tokens.push_back(bdecode_token(start - orig_start, 1
						, bdecode_token::end));

					// and back-patch the start of this sequence with the offset
					// to the next token we'll insert
					int top = stack[sp-1].token;
					// subtract the token's own index, since this is a relative
					// offset
					if (ret.m_tokens.size() - top > bdecode_token::max_next_item)
						TORRENT_FAIL_BDECODE(bdecode_errors::limit_exceeded);

					ret.m_tokens[top].next_item = ret.m_tokens.size() - top;

					// and pop it from the stack.
					--sp;
					++start;
					break;
				}
				default:
				{
					// this is the case for strings. The start character is any
					// numeric digit
					if (!numeric(t))
						TORRENT_FAIL_BDECODE(bdecode_errors::expected_value);

					boost::int64_t len = t - '0';
					char const* str_start = start;
					++start;
					bdecode_errors::error_code_enum e = bdecode_errors::no_error;
					start = parse_int(start, end, ':', len, e);
					if (e)
						TORRENT_FAIL_BDECODE(e);

					// remaining buffer size excluding ':'
					const ptrdiff_t buff_size = end - start - 1;
					if (len > buff_size)
						TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);
					if (len < 0)
						TORRENT_FAIL_BDECODE(bdecode_errors::overflow);

					// skip ':'
					++start;
					if (start >= end) TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);

					// the bdecode_token only has 8 bits to keep the header size
					// in. If it overflows, fail!
					if (start - str_start - 2 > detail::bdecode_token::max_header)
						TORRENT_FAIL_BDECODE(bdecode_errors::limit_exceeded);

					ret.m_tokens.push_back(bdecode_token(str_start - orig_start
						, 1, bdecode_token::string, start - str_start));
					start += len;
					break;
				}
			}

			if (current_frame > 0
				&& ret.m_tokens[stack[current_frame-1].token].type == bdecode_token::dict)
			{
				// the next item we parse is the opposite
				stack[current_frame-1].state = ~stack[current_frame-1].state;
			}

			// this terminates the top level node, we're done!
			if (sp == 0) break;
		}

done:

		// if parse failed, sp will be greater than 1
		// unwind the stack by inserting terminator to make whatever we have
		// so far valid
		while (sp > 0) {
			TORRENT_ASSERT(ec);
			--sp;

			// we may need to insert a dummy token to properly terminate the tree,
			// in case we just parsed a key to a dict and failed in the value
			if (ret.m_tokens[stack[sp].token].type == bdecode_token::dict
				&& stack[sp].state == 1)
			{
				// insert an empty dictionary as the value
				ret.m_tokens.push_back(bdecode_token(start - orig_start
					, 2, bdecode_token::dict));
				ret.m_tokens.push_back(bdecode_token(start - orig_start
					, bdecode_token::end));
			}

			int top = stack[sp].token;
			TORRENT_ASSERT(ret.m_tokens.size() - top <= bdecode_token::max_next_item);
			ret.m_tokens[top].next_item = ret.m_tokens.size() - top;
			ret.m_tokens.push_back(bdecode_token(start - orig_start, 1, bdecode_token::end));
		}

		ret.m_tokens.push_back(bdecode_token(start - orig_start, 0
			, bdecode_token::end));

		ret.m_token_idx = 0;
		ret.m_buffer = orig_start;
		ret.m_buffer_size = start - orig_start;
		ret.m_root_tokens = &ret.m_tokens[0];

		return ec ? -1 : 0;
	}