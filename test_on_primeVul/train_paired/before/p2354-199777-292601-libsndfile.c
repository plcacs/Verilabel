main (int argc, char **argv)
{	STATE state ;
	SF_INFO sfinfo ;
	char pathname [512], ext [32], *cptr ;
	int ch, double_split ;

	if (argc != 2)
	{	if (argc != 1)
			puts ("\nError : need a single input file.\n") ;
		usage_exit () ;
		} ;

	memset (&state, 0, sizeof (state)) ;
	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	if ((state.infile = sf_open (argv [1], SFM_READ, &sfinfo)) == NULL)
	{	printf ("\nError : Not able to open input file '%s'\n%s\n", argv [1], sf_strerror (NULL)) ;
		exit (1) ;
		} ;

	if (sfinfo.channels < 2)
	{	printf ("\nError : Input file '%s' only has one channel.\n", argv [1]) ;
		exit (1) ;
		} ;

	state.channels = sfinfo.channels ;
	sfinfo.channels = 1 ;

	if (snprintf (pathname, sizeof (pathname), "%s", argv [1]) > (int) sizeof (pathname))
	{	printf ("\nError : Length of provided filename '%s' exceeds MAX_PATH (%d).\n", argv [1], (int) sizeof (pathname)) ;
		exit (1) ;
		} ;

	if ((cptr = strrchr (pathname, '.')) == NULL)
		ext [0] = 0 ;
	else
	{	snprintf (ext, sizeof (ext), "%s", cptr) ;
		cptr [0] = 0 ;
		} ;

	printf ("Input file : %s\n", pathname) ;
	puts ("Output files :") ;

	for (ch = 0 ; ch < state.channels ; ch++)
	{	char filename [520] ;

		snprintf (filename, sizeof (filename), "%s_%02d%s", pathname, ch, ext) ;

		if ((state.outfile [ch] = sf_open (filename, SFM_WRITE, &sfinfo)) == NULL)
		{	printf ("Not able to open output file '%s'\n%s\n", filename, sf_strerror (NULL)) ;
			exit (1) ;
			} ;

		printf ("    %s\n", filename) ;
		} ;

	switch (sfinfo.format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_FLOAT :
		case SF_FORMAT_DOUBLE :
		case SF_FORMAT_VORBIS :
			double_split = 1 ;
			break ;

		default :
			double_split = 0 ;
			break ;
		} ;

	if (double_split)
		deinterleave_double (&state) ;
	else
		deinterleave_int (&state) ;

	sf_close (state.infile) ;
	for (ch = 0 ; ch < MAX_CHANNELS ; ch++)
		if (state.outfile [ch] != NULL)
			sf_close (state.outfile [ch]) ;

	return 0 ;
} /* main */