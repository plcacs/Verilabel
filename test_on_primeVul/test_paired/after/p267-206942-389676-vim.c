eval_string(char_u **arg, typval_T *rettv, int evaluate, int interpolate)
{
    char_u	*p;
    char_u	*end;
    int		extra = interpolate ? 1 : 0;
    int		off = interpolate ? 0 : 1;
    int		len;

    // Find the end of the string, skipping backslashed characters.
    for (p = *arg + off; *p != NUL && *p != '"'; MB_PTR_ADV(p))
    {
	if (*p == '\\' && p[1] != NUL)
	{
	    ++p;
	    // A "\<x>" form occupies at least 4 characters, and produces up
	    // to 9 characters (6 for the char and 3 for a modifier):
	    // reserve space for 5 extra.
	    if (*p == '<')
	    {
		int		modifiers = 0;
		int		flags = FSK_KEYCODE | FSK_IN_STRING;

		extra += 5;

		// Skip to the '>' to avoid using '{' inside for string
		// interpolation.
		if (p[1] != '*')
		    flags |= FSK_SIMPLIFY;
		if (find_special_key(&p, &modifiers, flags, NULL) != 0)
		    --p;  // leave "p" on the ">"
	    }
	}
	else if (interpolate && (*p == '{' || *p == '}'))
	{
	    if (*p == '{' && p[1] != '{') // start of expression
		break;
	    ++p;
	    if (p[-1] == '}' && *p != '}') // single '}' is an error
	    {
		semsg(_(e_stray_closing_curly_str), *arg);
		return FAIL;
	    }
	    --extra;  // "{{" becomes "{", "}}" becomes "}"
	}
    }

    if (*p != '"' && !(interpolate && *p == '{'))
    {
	semsg(_(e_missing_double_quote_str), *arg);
	return FAIL;
    }

    // If only parsing, set *arg and return here
    if (!evaluate)
    {
	*arg = p + off;
	return OK;
    }

    // Copy the string into allocated memory, handling backslashed
    // characters.
    rettv->v_type = VAR_STRING;
    len = (int)(p - *arg + extra);
    rettv->vval.v_string = alloc(len);
    if (rettv->vval.v_string == NULL)
	return FAIL;
    end = rettv->vval.v_string;

    for (p = *arg + off; *p != NUL && *p != '"'; )
    {
	if (*p == '\\')
	{
	    switch (*++p)
	    {
		case 'b': *end++ = BS; ++p; break;
		case 'e': *end++ = ESC; ++p; break;
		case 'f': *end++ = FF; ++p; break;
		case 'n': *end++ = NL; ++p; break;
		case 'r': *end++ = CAR; ++p; break;
		case 't': *end++ = TAB; ++p; break;

		case 'X': // hex: "\x1", "\x12"
		case 'x':
		case 'u': // Unicode: "\u0023"
		case 'U':
			  if (vim_isxdigit(p[1]))
			  {
			      int	n, nr;
			      int	c = toupper(*p);

			      if (c == 'X')
				  n = 2;
			      else if (*p == 'u')
				  n = 4;
			      else
				  n = 8;
			      nr = 0;
			      while (--n >= 0 && vim_isxdigit(p[1]))
			      {
				  ++p;
				  nr = (nr << 4) + hex2nr(*p);
			      }
			      ++p;
			      // For "\u" store the number according to
			      // 'encoding'.
			      if (c != 'X')
				  end += (*mb_char2bytes)(nr, end);
			      else
				  *end++ = nr;
			  }
			  break;

			  // octal: "\1", "\12", "\123"
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7': *end = *p++ - '0';
			  if (*p >= '0' && *p <= '7')
			  {
			      *end = (*end << 3) + *p++ - '0';
			      if (*p >= '0' && *p <= '7')
				  *end = (*end << 3) + *p++ - '0';
			  }
			  ++end;
			  break;

			  // Special key, e.g.: "\<C-W>"
		case '<':
			  {
			      int flags = FSK_KEYCODE | FSK_IN_STRING;

			      if (p[1] != '*')
				  flags |= FSK_SIMPLIFY;
			      extra = trans_special(&p, end, flags, FALSE, NULL);
			      if (extra != 0)
			      {
				  end += extra;
				  if (end >= rettv->vval.v_string + len)
				      iemsg("eval_string() used more space than allocated");
				  break;
			      }
			  }
			  // FALLTHROUGH

		default: MB_COPY_CHAR(p, end);
			  break;
	    }
	}
	else
	{
	    if (interpolate && (*p == '{' || *p == '}'))
	    {
		if (*p == '{' && p[1] != '{') // start of expression
		    break;
		++p;  // reduce "{{" to "{" and "}}" to "}"
	    }
	    MB_COPY_CHAR(p, end);
	}
    }
    *end = NUL;
    if (*p == '"' && !interpolate)
	++p;
    *arg = p;

    return OK;
}