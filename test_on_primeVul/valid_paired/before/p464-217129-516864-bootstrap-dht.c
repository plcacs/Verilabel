	int lazy_bdecode(char const* start, char const* end, lazy_entry& ret
		, error_code& ec, int* error_pos, int depth_limit, int item_limit)
	{
		char const* const orig_start = start;
		ret.clear();
		if (start == end) return 0;

		std::vector<lazy_entry*> stack;

		stack.push_back(&ret);
		while (start <= end)
		{
			if (stack.empty()) break; // done!

			lazy_entry* top = stack.back();

			if (int(stack.size()) > depth_limit) TORRENT_FAIL_BDECODE(bdecode_errors::depth_exceeded);
			if (start >= end) TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);
			char t = *start;
			++start;
			if (start >= end && t != 'e') TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);

			switch (top->type())
			{
				case lazy_entry::dict_t:
				{
					if (t == 'e')
					{
						top->set_end(start);
						stack.pop_back();
						continue;
					}
					if (!numeric(t)) TORRENT_FAIL_BDECODE(bdecode_errors::expected_string);
					boost::int64_t len = t - '0';
					bdecode_errors::error_code_enum e = bdecode_errors::no_error;
					start = parse_int(start, end, ':', len, e);
					if (e)
						TORRENT_FAIL_BDECODE(e);

					if (start + len + 1 > end)
						TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);

					if (len < 0)
						TORRENT_FAIL_BDECODE(bdecode_errors::overflow);

					++start;
					if (start == end) TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);
					lazy_entry* ent = top->dict_append(start);
					if (ent == 0) TORRENT_FAIL_BDECODE(boost::system::errc::not_enough_memory);
					start += len;
					if (start >= end) TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);
					stack.push_back(ent);
					t = *start;
					++start;
					break;
				}
				case lazy_entry::list_t:
				{
					if (t == 'e')
					{
						top->set_end(start);
						stack.pop_back();
						continue;
					}
					lazy_entry* ent = top->list_append();
					if (ent == 0) TORRENT_FAIL_BDECODE(boost::system::errc::not_enough_memory);
					stack.push_back(ent);
					break;
				}
				default: break;
			}

			--item_limit;
			if (item_limit <= 0) TORRENT_FAIL_BDECODE(bdecode_errors::limit_exceeded);

			top = stack.back();
			switch (t)
			{
				case 'd':
					top->construct_dict(start - 1);
					continue;
				case 'l':
					top->construct_list(start - 1);
					continue;
				case 'i':
				{
					char const* int_start = start;
					start = find_char(start, end, 'e');
					top->construct_int(int_start, start - int_start);
					if (start == end) TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);
					TORRENT_ASSERT(*start == 'e');
					++start;
					stack.pop_back();
					continue;
				}
				default:
				{
					if (!numeric(t))
						TORRENT_FAIL_BDECODE(bdecode_errors::expected_value);

					boost::int64_t len = t - '0';
					bdecode_errors::error_code_enum e = bdecode_errors::no_error;
					start = parse_int(start, end, ':', len, e);
					if (e)
						TORRENT_FAIL_BDECODE(e);
					if (start + len + 1 > end)
						TORRENT_FAIL_BDECODE(bdecode_errors::unexpected_eof);
					if (len < 0)
						TORRENT_FAIL_BDECODE(bdecode_errors::overflow);

					++start;
					top->construct_string(start, int(len));
					stack.pop_back();
					start += len;
					continue;
				}
			}
			return 0;
		}
		return 0;
	}