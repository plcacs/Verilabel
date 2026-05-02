cmdopts_t *cmdopts_parse(int argc, char **argv)
{
	enum {
		CMDOPT_HELP = 0,
		CMDOPT_VERBOSE,
		CMDOPT_QUIET,
		CMDOPT_INFILE,
		CMDOPT_INFMT,
		CMDOPT_INOPT,
		CMDOPT_OUTFILE,
		CMDOPT_OUTFMT,
		CMDOPT_OUTOPT,
		CMDOPT_VERSION,
		CMDOPT_DEBUG,
		CMDOPT_CMPTNO,
		CMDOPT_SRGB,
		CMDOPT_MAXMEM,
		CMDOPT_LIST_ENABLED_CODECS,
		CMDOPT_LIST_ALL_CODECS,
		CMDOPT_ENABLE_FORMAT,
		CMDOPT_ENABLE_ALL_FORMATS,
	};

	static const jas_opt_t cmdoptions[] = {
		{CMDOPT_HELP, "help", 0},
		{CMDOPT_VERBOSE, "verbose", 0},
		{CMDOPT_QUIET, "quiet", 0},
		{CMDOPT_QUIET, "q", 0},
		{CMDOPT_INFILE, "input", JAS_OPT_HASARG},
		{CMDOPT_INFILE, "f", JAS_OPT_HASARG},
		{CMDOPT_INFMT, "input-format", JAS_OPT_HASARG},
		{CMDOPT_INFMT, "t", JAS_OPT_HASARG},
		{CMDOPT_INOPT, "input-option", JAS_OPT_HASARG},
		{CMDOPT_INOPT, "o", JAS_OPT_HASARG},
		{CMDOPT_OUTFILE, "output", JAS_OPT_HASARG},
		{CMDOPT_OUTFILE, "F", JAS_OPT_HASARG},
		{CMDOPT_OUTFMT, "output-format", JAS_OPT_HASARG},
		{CMDOPT_OUTFMT, "T", JAS_OPT_HASARG},
		{CMDOPT_OUTOPT, "output-option", JAS_OPT_HASARG},
		{CMDOPT_OUTOPT, "O", JAS_OPT_HASARG},
		{CMDOPT_VERSION, "version", 0},
		{CMDOPT_DEBUG, "debug-level", JAS_OPT_HASARG},
		{CMDOPT_CMPTNO, "cmptno", JAS_OPT_HASARG},
		{CMDOPT_SRGB, "force-srgb", 0},
		{CMDOPT_SRGB, "S", 0},
		{CMDOPT_MAXMEM, "memory-limit", JAS_OPT_HASARG},
		{CMDOPT_LIST_ENABLED_CODECS, "list-enabled-formats", 0},
		{CMDOPT_LIST_ALL_CODECS, "list-all-formats", 0},
		{CMDOPT_ENABLE_FORMAT, "enable-format", JAS_OPT_HASARG},
		{CMDOPT_ENABLE_ALL_FORMATS, "enable-all-formats", 0},
		{-1, 0, 0}
	};

	cmdopts_t *cmdopts;
	int c;

	if (!(cmdopts = malloc(sizeof(cmdopts_t)))) {
		fprintf(stderr, "error: insufficient memory\n");
		exit(EXIT_FAILURE);
	}

	cmdopts->infile = 0;
	cmdopts->infmt = -1;
	cmdopts->infmt_str = 0;
	cmdopts->inopts = 0;
	cmdopts->inoptsbuf[0] = '\0';
	cmdopts->outfile = 0;
	cmdopts->outfmt = -1;
	cmdopts->outfmt_str = 0;
	cmdopts->outopts = 0;
	cmdopts->outoptsbuf[0] = '\0';
	cmdopts->verbose = 0;
	cmdopts->version = 0;
	cmdopts->cmptno = -1;
	cmdopts->debug = 0;
	cmdopts->srgb = 0;
	cmdopts->list_codecs = 0;
	cmdopts->list_codecs_all = 0;
	cmdopts->help = 0;
	cmdopts->max_mem = get_default_max_mem_usage();
	cmdopts->enable_format = 0;
	cmdopts->enable_all_formats = 0;

	while ((c = jas_getopt(argc, argv, cmdoptions)) != EOF) {
		switch (c) {
		case CMDOPT_HELP:
			cmdopts->help = 1;
			break;
		case CMDOPT_VERBOSE:
			cmdopts->verbose = 1;
			break;
		case CMDOPT_QUIET:
			cmdopts->verbose = -1;
			break;
		case CMDOPT_VERSION:
			cmdopts->version = 1;
			break;
		case CMDOPT_LIST_ENABLED_CODECS:
			cmdopts->list_codecs = 1;
			cmdopts->list_codecs_all = 0;
			break;
		case CMDOPT_LIST_ALL_CODECS:
			cmdopts->list_codecs = 1;
			cmdopts->list_codecs_all = 1;
			break;
		case CMDOPT_DEBUG:
			cmdopts->debug = atoi(jas_optarg);
			break;
		case CMDOPT_INFILE:
			cmdopts->infile = jas_optarg;
			break;
		case CMDOPT_INFMT:
			cmdopts->infmt_str= jas_optarg;
			break;
		case CMDOPT_INOPT:
			addopt(cmdopts->inoptsbuf, OPTSMAX, jas_optarg);
			cmdopts->inopts = cmdopts->inoptsbuf;
			break;
		case CMDOPT_OUTFILE:
			cmdopts->outfile = jas_optarg;
			break;
		case CMDOPT_OUTFMT:
			cmdopts->outfmt_str = jas_optarg;
			break;
		case CMDOPT_OUTOPT:
			addopt(cmdopts->outoptsbuf, OPTSMAX, jas_optarg);
			cmdopts->outopts = cmdopts->outoptsbuf;
			break;
		case CMDOPT_CMPTNO:
			cmdopts->cmptno = atoi(jas_optarg);
			break;
		case CMDOPT_SRGB:
			cmdopts->srgb = 1;
			break;
		case CMDOPT_MAXMEM:
			cmdopts->max_mem = strtoull(jas_optarg, 0, 10);
			break;
		case CMDOPT_ENABLE_FORMAT:
			cmdopts->enable_format = jas_optarg;
			break;
		case CMDOPT_ENABLE_ALL_FORMATS:
			cmdopts->enable_all_formats = 1;
			break;
		default:
			cmdopts_destroy(cmdopts);
			badusage();
			break;
		}
	}

	while (jas_optind < argc) {
		fprintf(stderr,
		  "warning: ignoring bogus command line argument %s\n",
		  argv[jas_optind]);
		++jas_optind;
	}

	if (cmdopts->version || cmdopts->list_codecs || cmdopts->help) {
		goto done;
	}

	if (!cmdopts->outfmt_str && !cmdopts->outfile) {
		fprintf(stderr, "error: cannot determine output format\n");
		cmdopts_destroy(cmdopts);
		badusage();
	}

done:
	return cmdopts;
}