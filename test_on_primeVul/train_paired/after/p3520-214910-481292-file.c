file_printable(char *buf, size_t bufsiz, const char *str)
{
	char *ptr, *eptr;
	const unsigned char *s = (const unsigned char *)str;

	for (ptr = buf, eptr = ptr + bufsiz - 1; ptr < eptr && *s; s++) {
		if (isprint(*s)) {
			*ptr++ = *s;
			continue;
		}
		if (ptr >= eptr - 3)
			break;
		*ptr++ = '\\';
		*ptr++ = ((*s >> 6) & 7) + '0';
		*ptr++ = ((*s >> 3) & 7) + '0';
		*ptr++ = ((*s >> 0) & 7) + '0';
	}
	*ptr = '\0';
	return buf;
}