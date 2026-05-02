table_regex_match(const char *string, const char *pattern)
{
	regex_t preg;
	int	cflags = REG_EXTENDED|REG_NOSUB;
	int ret;

	if (strncmp(pattern, "(?i)", 4) == 0) {
		cflags |= REG_ICASE;
		pattern += 4;
	}

	if (regcomp(&preg, pattern, cflags) != 0)
		return (0);

	ret = regexec(&preg, string, 0, NULL, 0);

	regfree(&preg);

	if (ret != 0)
		return (0);

	return (1);
}