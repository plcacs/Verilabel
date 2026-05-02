bool initialise_control(rzip_control *control)
{
	time_t now_t, tdiff;
	char localeptr[] = "./", *eptr; /* for environment */
	size_t len;

	memset(control, 0, sizeof(rzip_control));
	control->msgout = stderr;
	control->msgerr = stderr;
	register_outputfile(control, control->msgout);
	control->flags = FLAG_SHOW_PROGRESS | FLAG_KEEP_FILES | FLAG_THRESHOLD;
	control->suffix = ".lrz";
	control->compression_level = 7;
	control->ramsize = get_ram(control);
	if (unlikely(control->ramsize == -1))
		return false;
	/* for testing single CPU */
	control->threads = PROCESSORS;	/* get CPUs for LZMA */
	control->page_size = PAGE_SIZE;
	control->nice_val = 19;

	/* The first 5 bytes of the salt is the time in seconds.
	 * The next 2 bytes encode how many times to hash the password.
	 * The last 9 bytes are random data, making 16 bytes of salt */
	if (unlikely((now_t = time(NULL)) == ((time_t)-1)))
		fatal_return(("Failed to call time in main\n"), false);
	if (unlikely(now_t < T_ZERO)) {
		print_output("Warning your time reads before the year 2011, check your system clock\n");
		now_t = T_ZERO;
	}
	/* Workaround for CPUs no longer keeping up with Moore's law!
	 * This way we keep the magic header format unchanged. */
	tdiff = (now_t - T_ZERO) / 4;
	now_t = T_ZERO + tdiff;
	control->secs = now_t;
	control->encloops = nloops(control->secs, control->salt, control->salt + 1);
	if (unlikely(!get_rand(control, control->salt + 2, 6)))
		return false;

	/* Get Temp Dir. Try variations on canonical unix environment variable */
	eptr = getenv("TMPDIR");
	if (!eptr)
		eptr = getenv("TMP");
	if (!eptr)
		eptr = getenv("TEMPDIR");
	if (!eptr)
		eptr = getenv("TEMP");
	if (!eptr)
		eptr = localeptr;
	len = strlen(eptr);

	control->tmpdir = malloc(len + 2);
	if (control->tmpdir == NULL)
		fatal_return(("Failed to allocate for tmpdir\n"), false);
	strcpy(control->tmpdir, eptr);
	if (control->tmpdir[len - 1] != '/') {
		control->tmpdir[len] = '/'; /* need a trailing slash */
		control->tmpdir[len + 1] = '\0';
	}
	return true;
}