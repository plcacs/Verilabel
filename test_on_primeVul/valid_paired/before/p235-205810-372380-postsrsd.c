srs_timestamp_check(srs_t *srs, const char *stamp)
{
	const char	*sp;
	char		*bp;
	int			 off;
	time_t		 now;
	time_t		 then;

	/* We had better go around this loop exactly twice! */
	then = 0;
	for (sp = stamp; *sp; sp++) {
		bp = strchr(SRS_TIME_BASECHARS, toupper(*sp));
		if (bp == NULL)
			return SRS_EBADTIMESTAMPCHAR;
		off = bp - SRS_TIME_BASECHARS;
		then = (then << SRS_TIME_BASEBITS) | off;
	}

	time(&now);
	now = (now / SRS_TIME_PRECISION) % SRS_TIME_SLOTS;
	while (now < then)
		now = now + SRS_TIME_SLOTS;

	if (now <= then + srs->maxage)
		return SRS_SUCCESS;
	return SRS_ETIMESTAMPOUTOFDATE;
}