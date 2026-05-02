cvtchar(register const char *sp)
/* convert a character to a terminfo push */
{
    unsigned char c = 0;
    int len;

    switch (*sp) {
    case '\\':
	switch (*++sp) {
	case '\'':
	case '$':
	case '\\':
	case '%':
	    c = UChar(*sp);
	    len = 2;
	    break;
	case '\0':
	    c = '\\';
	    len = 1;
	    break;
	case '0':
	case '1':
	case '2':
	case '3':
	    len = 1;
	    while (isdigit(UChar(*sp))) {
		c = UChar(8 * c + (*sp++ - '0'));
		len++;
	    }
	    break;
	default:
	    c = UChar(*sp);
	    len = (c != '\0') ? 2 : 1;
	    break;
	}
	break;
    case '^':
	c = UChar(*++sp);
	if (c == '?')
	    c = 127;
	else
	    c &= 0x1f;
	len = 2;
	break;
    default:
	c = UChar(*sp);
	len = (c != '\0') ? 1 : 0;
    }
    if (isgraph(c) && c != ',' && c != '\'' && c != '\\' && c != ':') {
	dp = save_string(dp, "%\'");
	dp = save_char(dp, c);
	dp = save_char(dp, '\'');
    } else if (c != '\0') {
	dp = save_string(dp, "%{");
	if (c > 99)
	    dp = save_char(dp, c / 100 + '0');
	if (c > 9)
	    dp = save_char(dp, ((int) (c / 10)) % 10 + '0');
	dp = save_char(dp, c % 10 + '0');
	dp = save_char(dp, '}');
    }
    return len;
}