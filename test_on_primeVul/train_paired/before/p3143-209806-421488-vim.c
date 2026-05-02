skip_string(char_u *p)
{
    int	    i;

    // We loop, because strings may be concatenated: "date""time".
    for ( ; ; ++p)
    {
	if (p[0] == '\'')		    // 'c' or '\n' or '\000'
	{
	    if (p[1] == NUL)		    // ' at end of line
		break;
	    i = 2;
	    if (p[1] == '\\' && p[2] != NUL)    // '\n' or '\000'
	    {
		++i;
		while (vim_isdigit(p[i - 1]))   // '\000'
		    ++i;
	    }
	    if (p[i] == '\'')		    // check for trailing '
	    {
		p += i;
		continue;
	    }
	}
	else if (p[0] == '"')		    // start of string
	{
	    for (++p; p[0]; ++p)
	    {
		if (p[0] == '\\' && p[1] != NUL)
		    ++p;
		else if (p[0] == '"')	    // end of string
		    break;
	    }
	    if (p[0] == '"')
		continue; // continue for another string
	}
	else if (p[0] == 'R' && p[1] == '"')
	{
	    // Raw string: R"[delim](...)[delim]"
	    char_u *delim = p + 2;
	    char_u *paren = vim_strchr(delim, '(');

	    if (paren != NULL)
	    {
		size_t delim_len = paren - delim;

		for (p += 3; *p; ++p)
		    if (p[0] == ')' && STRNCMP(p + 1, delim, delim_len) == 0
			    && p[delim_len + 1] == '"')
		    {
			p += delim_len + 1;
			break;
		    }
		if (p[0] == '"')
		    continue; // continue for another string
	    }
	}
	break;				    // no string found
    }
    if (!*p)
	--p;				    // backup from NUL
    return p;
}