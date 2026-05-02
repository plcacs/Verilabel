nextvar(
	int *datalen,
	const char **datap,
	char **vname,
	char **vvalue
	)
{
	const char *cp;
	const char *np;
	const char *cpend;
	size_t srclen;
	size_t len;
	static char name[MAXVARLEN];
	static char value[MAXVALLEN];

	cp = *datap;
	cpend = cp + *datalen;

	/*
	 * Space past commas and white space
	 */
	while (cp < cpend && (*cp == ',' || isspace((int)*cp)))
		cp++;
	if (cp >= cpend)
		return 0;

	/*
	 * Copy name until we hit a ',', an '=', a '\r' or a '\n'.  Backspace
	 * over any white space and terminate it.
	 */
	srclen = strcspn(cp, ",=\r\n");
	srclen = min(srclen, (size_t)(cpend - cp));
	len = srclen;
	while (len > 0 && isspace((unsigned char)cp[len - 1]))
		len--;
	if (len > 0)
		memcpy(name, cp, len);
	name[len] = '\0';
	*vname = name;
	cp += srclen;

	/*
	 * Check if we hit the end of the buffer or a ','.  If so we are done.
	 */
	if (cp >= cpend || *cp == ',' || *cp == '\r' || *cp == '\n') {
		if (cp < cpend)
			cp++;
		*datap = cp;
		*datalen = cpend - cp;
		*vvalue = NULL;
		return 1;
	}

	/*
	 * So far, so good.  Copy out the value
	 */
	cp++;	/* past '=' */
	while (cp < cpend && (isspace((unsigned char)*cp) && *cp != '\r' && *cp != '\n'))
		cp++;
	np = cp;
	if ('"' == *np) {
		do {
			np++;
		} while (np < cpend && '"' != *np);
		if (np < cpend && '"' == *np)
			np++;
	} else {
		while (np < cpend && ',' != *np && '\r' != *np)
			np++;
	}
	len = np - cp;
	if (np > cpend || len >= sizeof(value) ||
	    (np < cpend && ',' != *np && '\r' != *np))
		return 0;
	memcpy(value, cp, len);
	/*
	 * Trim off any trailing whitespace
	 */
	while (len > 0 && isspace((unsigned char)value[len - 1]))
		len--;
	value[len] = '\0';

	/*
	 * Return this.  All done.
	 */
	if (np < cpend && ',' == *np)
		np++;
	*datap = np;
	*datalen = cpend - np;
	*vvalue = value;
	return 1;
}