table_regex_match(const char *string, const char *pattern)
{
	regex_t preg;
	int	cflags = REG_EXTENDED|REG_NOSUB;

	if (strncmp(pattern, "(?i)", 4) == 0) {
		cflags |= REG_ICASE;
		pattern += 4;
	}

	if (regcomp(&preg, pattern, cflags) != 0)
		return (0);

	if (regexec(&preg, string, 0, NULL, 0) != 0)
		return (0);

	return (1);
}