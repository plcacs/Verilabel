  char * unescape(char * dest, const char * src)
  {
    while (*src) {
      if (*src == '\\') {
	++src;
	switch (*src) {
	case 'n': *dest = '\n'; break;
	case 'r': *dest = '\r'; break;
	case 't': *dest = '\t'; break;
	case 'f': *dest = '\f'; break;
	case 'v': *dest = '\v'; break;
	default: *dest = *src;
	}
      } else {
	*dest = *src;
      }
      ++src;
      ++dest;
    }
    *dest = '\0';
    return dest;
  }