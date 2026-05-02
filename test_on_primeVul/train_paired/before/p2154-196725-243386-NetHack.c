escapes(cp, tp)
const char	*cp;
char *tp;
{
    while (*cp) {
	int	cval = 0, meta = 0;

	if (*cp == '\\' && cp[1] && index("mM", cp[1]) && cp[2]) {
		meta = 1;
		cp += 2;
	}
	if (*cp == '\\' && cp[1] && index("0123456789xXoO", cp[1]) && cp[2]) {
	    NEARDATA const char hex[] = "00112233445566778899aAbBcCdDeEfF";
	    const char *dp;
	    int dcount = 0;

	    cp++;
	    if (*cp == 'x' || *cp == 'X')
		for (++cp; *cp && (dp = index(hex, *cp)) && (dcount++ < 2); cp++)
		    cval = (cval * 16) + ((int)(dp - hex) / 2);
	    else if (*cp == 'o' || *cp == 'O')
		for (++cp; *cp && (index("01234567",*cp)) && (dcount++ < 3); cp++)
		    cval = (cval * 8) + (*cp - '0');
	    else
		for (; *cp && (index("0123456789",*cp)) && (dcount++ < 3); cp++)
		    cval = (cval * 10) + (*cp - '0');
	} else if (*cp == '\\' && cp[1]) {	/* C-style character escapes */
	    switch (*++cp) {
	    case '\\': cval = '\\'; break;
	    case 'n': cval = '\n'; break;
	    case 't': cval = '\t'; break;
	    case 'b': cval = '\b'; break;
	    case 'r': cval = '\r'; break;
	    default: cval = *cp;
	    }
	    cp++;
	} else if (*cp == '^' && cp[1]) { /* expand control-character syntax */
	    cval = (*++cp & 0x1f);
	    cp++;
	} else
	    cval = *cp++;

	if (meta)
	    cval |= 0x80;
	*tp++ = cval;
    }
    *tp = '\0';
}