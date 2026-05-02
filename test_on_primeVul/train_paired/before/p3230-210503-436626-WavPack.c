int main(int argc, char **argv)
#endif
{
#ifdef __EMX__ /* OS/2 */
    _wildcard (&argc, &argv);
#endif
    int verify_only = 0, error_count = 0, add_extension = 0, output_spec = 0, c_count = 0, x_count = 0;
    char outpath, **matches = NULL, *outfilename = NULL, **argv_fn = NULL, selfname [PATH_MAX];
    int use_stdin = 0, use_stdout = 0, argc_fn = 0, argi, result;

#if defined(_WIN32)
    if (!GetModuleFileName (NULL, selfname, sizeof (selfname)))
#endif
    strncpy (selfname, *argv, sizeof (selfname));

    if (filespec_name (selfname)) {
        char *filename = filespec_name (selfname);

        if (strstr (filename, "ebug") || strstr (filename, "DEBUG"))
            debug_logging_mode = TRUE;

        while (strchr (filename, '{')) {
            char *open_brace = strchr (filename, '{');
            char *close_brace = strchr (open_brace, '}');

            if (!close_brace)
                break;

            if (close_brace - open_brace > 1) {
                int option_len = (int)(close_brace - open_brace) - 1;
                char *option = malloc (option_len + 1);

                argv_fn = realloc (argv_fn, sizeof (char *) * ++argc_fn);
                memcpy (option, open_brace + 1, option_len);
                argv_fn [argc_fn - 1] = option;
                option [option_len] = 0;

                if (debug_logging_mode)
                    error_line ("file arg %d: %s", argc_fn, option);
            }

            filename = close_brace;
        }
    }

    if (debug_logging_mode) {
        char **argv_t = argv;
        int argc_t = argc;

        while (--argc_t)
            error_line ("cli arg %d: %s", argc - argc_t, *++argv_t);
    }

#if defined (_WIN32)
    set_console_title = 1;      // on Windows, we default to messing with the console title
#endif                          // on Linux, this is considered uncool to do by default

    // loop through command-line arguments

    for (argi = 0; argi < argc + argc_fn - 1; ++argi) {
        char *argcp;

        if (argi < argc_fn)
            argcp = argv_fn [argi];
        else
            argcp = argv [argi - argc_fn + 1];

        if (argcp [0] == '-' && argcp [1] == '-' && argcp [2]) {
            char *long_option = argcp + 2, *long_param = long_option;

            while (*long_param)
                if (*long_param++ == '=')
                    break;

            if (!strcmp (long_option, "help")) {                        // --help
                printf ("%s", help);
                return 0;
            }
            else if (!strcmp (long_option, "version")) {                // --version
                printf ("wvunpack %s\n", PACKAGE_VERSION);
                printf ("libwavpack %s\n", WavpackGetLibraryVersionString ());
                return 0;
            }
#ifdef _WIN32
            else if (!strcmp (long_option, "pause"))                    // --pause
                pause_mode = 1;
            else if (!strcmp (long_option, "drop"))                     // --drop
                drop_mode = 1;
#endif
            else if (!strcmp (long_option, "normalize-floats"))         // --normalize-floats
                normalize_floats = 1;
            else if (!strcmp (long_option, "no-utf8-convert"))          // --no-utf8-convert
                no_utf8_convert = 1;
            else if (!strncmp (long_option, "skip", 4)) {               // --skip
                parse_sample_time_index (&skip, long_param);

                if (!skip.value_is_valid) {
                    error_line ("invalid --skip parameter!");
                    ++error_count;
                }
            }
            else if (!strncmp (long_option, "until", 5)) {              // --until
                parse_sample_time_index (&until, long_param);

                if (!until.value_is_valid) {
                    error_line ("invalid --until parameter!");
                    ++error_count;
                }
            }
            else if (!strcmp (long_option, "caf-be")) {                 // --caf-be
                decode_format = WP_FORMAT_CAF;
                caf_be = format_specified = 1;
            }
            else if (!strcmp (long_option, "caf-le")) {                 // --caf-le
                decode_format = WP_FORMAT_CAF;
                format_specified = 1;
            }
            else if (!strcmp (long_option, "dsf")) {                    // --dsf
                decode_format = WP_FORMAT_DSF;
                format_specified = 1;
            }
            else if (!strcmp (long_option, "dsdiff") || !strcmp (long_option, "dff")) {
                decode_format = WP_FORMAT_DFF;                          // --dsdiff or --dff
                format_specified = 1;
            }
            else if (!strcmp (long_option, "w64")) {                    // --w64
                decode_format = WP_FORMAT_W64;
                format_specified = 1;
            }
            else if (!strcmp (long_option, "wav")) {                    // --wav
                decode_format = WP_FORMAT_WAV;
                format_specified = 1;
            }
            else if (!strcmp (long_option, "raw-pcm"))                  // --raw-pcm
                raw_pcm = raw_decode = 1;
            else if (!strcmp (long_option, "raw"))                      // --raw
                raw_decode = 1;
            else {
                error_line ("unknown option: %s !", long_option);
                ++error_count;
            }
        }
#if defined (_WIN32)
        else if ((argcp [0] == '-' || argcp [0] == '/') && argcp [1])
#else
        else if (argcp [0] == '-' && argcp [1])
#endif
            while (*++argcp)
                switch (*argcp) {
                    case 'Y': case 'y':
                        overwrite_all = 1;
                        break;

                    case 'C': case 'c':
                        if (++c_count == 2) {
                            add_tag_extraction_to_list ("cuesheet=%a.cue");
                            c_count = 0;
                        }

                        break;

                    case 'D': case 'd':
                        delete_source = 1;
                        break;

#if defined (_WIN32)
                    case 'L': case 'l':
                        SetPriorityClass (GetCurrentProcess(), IDLE_PRIORITY_CLASS);
                        break;
#elif defined (__OS2__)
                    case 'L': case 'l':
                        DosSetPriority (0, PRTYC_IDLETIME, 0, 0);
                        break;
#endif
#if defined (_WIN32)
                    case 'O': case 'o':  // ignore -o in Windows to be Linux compatible
                        break;
#else
                    case 'O': case 'o':
                        output_spec = 1;
                        break;
#endif
                    case 'T': case 't':
                        copy_time = 1;
                        break;

                    case 'V': case 'v':
                        ++verify_only;
                        break;

                    case 'F': case 'f':
                        file_info = (char) strtol (++argcp, &argcp, 10);

                        if (file_info < 0 || file_info > 10) {
                            error_line ("-f option must be 1-10, or omit (or 0) for all!");
                            ++error_count;
                        }
                        else {
                            quiet_mode = no_audio_decode = 1;
                            file_info++;
                        }

                        --argcp;
                        break;

                    case 'S': case 's':
                        no_audio_decode = 1;
                        ++summary;
                        break;

                    case 'K': case 'k':
                        outbuf_k = strtol (++argcp, &argcp, 10);

                        if (outbuf_k < 1 || outbuf_k > 16384)       // range-check for reasonable values
                            outbuf_k = 0;

                        --argcp;
                        break;

                    case 'M': case 'm':
                        calc_md5 = 1;
                        break;

                    case 'B': case 'b':
                        blind_decode = 1;
                        break;

                    case 'N': case 'n':
                        no_audio_decode = 1;
                        break;

                    case 'R': case 'r':
                        raw_decode = 1;
                        break;

                    case 'W': case 'w':
                        decode_format = WP_FORMAT_WAV;
                        format_specified = 1;
                        break;

                    case 'Q': case 'q':
                        quiet_mode = 1;
                        break;

                    case 'Z': case 'z':
                        set_console_title = (char) strtol (++argcp, &argcp, 10);
                        --argcp;
                        break;

                    case 'X': case 'x':
                        if (++x_count == 3) {
                            error_line ("illegal option: %s !", argcp);
                            ++error_count;
                            x_count = 0;
                        }

                        break;

                    case 'I': case 'i':
                        ignore_wvc = 1;
                        break;

                    default:
                        error_line ("illegal option: %c !", *argcp);
                        ++error_count;
                }
        else if (argi < argc_fn) {
            error_line ("invalid use of filename-embedded args: %s !", argcp);
            ++error_count;
        }
        else {
            if (x_count) {
                if (x_count == 1) {
                    if (tag_extract_stdout) {
                        error_line ("can't extract more than 1 tag item to stdout at a time!");
                        ++error_count;
                    }
                    else {
                        tag_extract_stdout = argcp;
                        no_audio_decode = 1;
                    }
                }
                else if (x_count == 2)
                    add_tag_extraction_to_list (argcp);

                x_count = 0;
            }
#if defined (_WIN32)
            else if (drop_mode || !num_files) {
                matches = realloc (matches, (num_files + 1) * sizeof (*matches));
                matches [num_files] = malloc (strlen (argcp) + 10);
                strcpy (matches [num_files], argcp);
                use_stdin |= (*argcp == '-');

                if (*(matches [num_files]) != '-' && *(matches [num_files]) != '@' &&
                    !filespec_ext (matches [num_files]))
                        strcat (matches [num_files], ".wv");

                num_files++;
            }
            else if (!outfilename) {
                outfilename = malloc (strlen (argcp) + PATH_MAX);
                strcpy (outfilename, argcp);
                use_stdout = (*argcp == '-');
            }
            else {
                error_line ("extra unknown argument: %s !", argcp);
                ++error_count;
            }
#else
            else if (output_spec) {
                outfilename = malloc (strlen (argcp) + PATH_MAX);
                strcpy (outfilename, argcp);
                use_stdout = (*argcp == '-');
                output_spec = 0;
            }
            else {
                matches = realloc (matches, (num_files + 1) * sizeof (*matches));
                matches [num_files] = malloc (strlen (argcp) + 10);
                strcpy (matches [num_files], argcp);
                use_stdin |= (*argcp == '-');

                if (*(matches [num_files]) != '-' && *(matches [num_files]) != '@' &&
                    !filespec_ext (matches [num_files]))
                        strcat (matches [num_files], ".wv");

                num_files++;
            }
#endif
        }

        if (argi < argc_fn)
            free (argv_fn [argi]);
    }

    free (argv_fn);

   // check for various command-line argument problems

    if (output_spec) {
        error_line ("no output filename or path specified with -o option!");
        ++error_count;
    }

    if (use_stdin && num_files > 1) {
        error_line ("when stdin is used for input, it must be the only file!");
        ++error_count;
    }

    if (use_stdin && !outfilename)  // for stdin source, no output specification implies stdout
        use_stdout = 1;

    if (delete_source && (verify_only || skip.value_is_valid || until.value_is_valid)) {
        error_line ("can't delete in verify mode or when --skip or --until are used!");
        delete_source = 0;
    }

    if (raw_decode && format_specified) {
        error_line ("-r (raw decode) and specifying a format (like -w) are incompatible!");
        ++error_count;
    }

    if (verify_only && (format_specified || outfilename)) {
        error_line ("specifying output file or format and verify mode are incompatible!");
        ++error_count;
    }

    if (verify_only > 1 && calc_md5) {
        error_line ("can't calculate MD5s in quick verify mode!");
        ++error_count;
    }

    if (c_count == 1) {
        if (tag_extract_stdout) {
            error_line ("can't extract more than 1 tag item to stdout at a time!");
            error_count++;
        }
        else {
            tag_extract_stdout = "cuesheet";
            no_audio_decode = 1;
        }
    }

    if ((summary || file_info) && (num_tag_extractions || outfilename || verify_only || delete_source || format_specified)) {
        error_line ("can't display file information and do anything else!");
        ++error_count;
    }

    if (tag_extract_stdout && (num_tag_extractions || outfilename || verify_only || delete_source || format_specified || raw_decode)) {
        error_line ("can't extract a tag to stdout and do anything else!");
        ++error_count;
    }

    if ((tag_extract_stdout || num_tag_extractions) && use_stdout) {
        error_line ("can't extract tags when unpacking audio to stdout!");
        ++error_count;
    }

    if (strcmp (WavpackGetLibraryVersionString (), PACKAGE_VERSION)) {
        fprintf (stderr, version_warning, WavpackGetLibraryVersionString (), PACKAGE_VERSION);
        fflush (stderr);
    }
    else if (!quiet_mode && !error_count) {
        fprintf (stderr, sign_on, VERSION_OS, WavpackGetLibraryVersionString ());
        fflush (stderr);
    }

    if (error_count) {
        fprintf (stderr, "\ntype 'wvunpack' for short help or 'wvunpack --help' for full help\n");
        fflush (stderr);
        return 1;
    }

    if (!num_files) {
        printf ("%s", usage);
        return 1;
    }

    setup_break ();

    for (file_index = 0; file_index < num_files; ++file_index) {
        char *infilename = matches [file_index];

        // If the single infile specification begins with a '@', then it
        // actually points to a file that contains the names of the files
        // to be converted. This was included for use by Wim Speekenbrink's
        // frontends, but could be used for other purposes.

        if (*infilename == '@') {
            FILE *list = fopen (infilename+1, "rb");
            char *listbuff = NULL, *cp;
            int listbytes = 0, di, c;

            for (di = file_index; di < num_files - 1; di++)
                matches [di] = matches [di + 1];

            file_index--;
            num_files--;

            if (list == NULL) {
                error_line ("file %s not found!", infilename+1);
                free (infilename);
                return 1;
            }

            while (1) {
                int bytes_read;

                listbuff = realloc (listbuff, listbytes + 1024);
                memset (listbuff + listbytes, 0, 1024);
                listbytes += bytes_read = (int) fread (listbuff + listbytes, 1, 1024, list);

                if (bytes_read < 1024)
                    break;
            }

#if defined (_WIN32)
            listbuff = realloc (listbuff, listbytes *= 2);
            TextToUTF8 (listbuff, listbytes);
#endif
            cp = listbuff;

            while ((c = *cp++)) {

                while (c == '\n' || c == '\r')
                    c = *cp++;

                if (c) {
                    char *fname = malloc (PATH_MAX);
                    int ci = 0;

                    do
                        fname [ci++] = c;
                    while ((c = *cp++) != '\n' && c != '\r' && c && ci < PATH_MAX);

                    fname [ci++] = '\0';
                    matches = realloc (matches, ++num_files * sizeof (*matches));

                    for (di = num_files - 1; di > file_index + 1; di--)
                        matches [di] = matches [di - 1];

                    matches [++file_index] = fname;
                }

                if (!c)
                    break;
            }

            fclose (list);
            free (listbuff);
            free (infilename);
        }
#if defined (_WIN32)
        else if (filespec_wild (infilename)) {
            wchar_t *winfilename = utf8_to_utf16(infilename);
            struct _wfinddata_t _wfinddata_t;
            intptr_t file;
            int di;

            for (di = file_index; di < num_files - 1; di++)
                matches [di] = matches [di + 1];

            file_index--;
            num_files--;

            if ((file = _wfindfirst (winfilename, &_wfinddata_t)) != (intptr_t) -1) {
                do {
                    char *name_utf8;

                    if (!(_wfinddata_t.attrib & _A_SUBDIR) && (name_utf8 = utf16_to_utf8(_wfinddata_t.name))) {
                        matches = realloc (matches, ++num_files * sizeof (*matches));

                        for (di = num_files - 1; di > file_index + 1; di--)
                            matches [di] = matches [di - 1];

                        matches [++file_index] = malloc (strlen (infilename) + strlen (name_utf8) + 10);
                        strcpy (matches [file_index], infilename);
                        *filespec_name (matches [file_index]) = '\0';
                        strcat (matches [file_index], name_utf8);
                        free (name_utf8);
                    }
                } while (_wfindnext (file, &_wfinddata_t) == 0);

                _findclose (file);
            }

            free (winfilename);
            free (infilename);
        }
#endif
    }

    // If the outfile specification begins with a '@', then it actually points
    // to a file that contains the output specification. This was included for
    // use by Wim Speekenbrink's frontends because certain filenames could not
    // be passed on the command-line, but could be used for other purposes.

    if (outfilename && outfilename [0] == '@') {
        char listbuff [PATH_MAX * 2], *lp = listbuff;
        FILE *list = fopen (outfilename+1, "rb");
        int c;

        if (list == NULL) {
            error_line ("file %s not found!", outfilename+1);
            free(outfilename);
            return 1;
        }

        memset (listbuff, 0, sizeof (listbuff));
        c = (int) fread (listbuff, 1, sizeof (listbuff) - 1, list);   // assign c only to suppress warning

#if defined (_WIN32)
        TextToUTF8 (listbuff, PATH_MAX * 2);
#endif

        while ((c = *lp++) == '\n' || c == '\r');

        if (c) {
            int ci = 0;

            do
                outfilename [ci++] = c;
            while ((c = *lp++) != '\n' && c != '\r' && c && ci < PATH_MAX);

            outfilename [ci] = '\0';
        }
        else {
            error_line ("output spec file is empty!");
            free(outfilename);
            fclose (list);
            return 1;
        }

        fclose (list);
    }

    // if we found any files to process, this is where we start

    if (num_files) {
        if (outfilename && *outfilename != '-') {
            outpath = (filespec_path (outfilename) != NULL);

            if (num_files > 1 && !outpath) {
                error_line ("%s is not a valid output path", outfilename);
                free (outfilename);
                return 1;
            }
        }
        else
            outpath = 0;

        add_extension = !outfilename || outpath || !filespec_ext (outfilename);

        // loop through and process files in list

        for (file_index = 0; file_index < num_files; ++file_index) {
            if (check_break ())
                break;

            // generate output filename

            if (outpath) {
                strcat (outfilename, filespec_name (matches [file_index]));

                if (filespec_ext (outfilename))
                    *filespec_ext (outfilename) = '\0';
            }
            else if (!outfilename) {
                outfilename = malloc (strlen (matches [file_index]) + 10);
                strcpy (outfilename, matches [file_index]);

                if (filespec_ext (outfilename))
                    *filespec_ext (outfilename) = '\0';
            }

            if (num_files > 1 && !quiet_mode) {
                fprintf (stderr, "\n%s:\n", matches [file_index]);
                fflush (stderr);
            }

            if (verify_only > 1) {
                result = quick_verify_file (matches [file_index], verify_only > 2);

                // quick_verify_file() returns hard error to mean file cannot be quickly verified
                // because it has no block checksums, so fall back to standard slow verify

                if (result == WAVPACK_HARD_ERROR)
                    result = unpack_file (matches [file_index], NULL, 0);
            }
            else
                result = unpack_file (matches [file_index], verify_only ? NULL : outfilename, add_extension);

            if (result != WAVPACK_NO_ERROR)
                ++error_count;

            if (result == WAVPACK_HARD_ERROR)
                break;

            // clean up in preparation for potentially another file

            if (outpath)
                *filespec_name (outfilename) = '\0';
            else if (*outfilename != '-') {
                free (outfilename);
                outfilename = NULL;
            }

            free (matches [file_index]);
        }

        if (num_files > 1) {
            if (error_count) {
                fprintf (stderr, "\n **** warning: errors occurred in %d of %d files! ****\n", error_count, num_files);
                fflush (stderr);
            }
            else if (!quiet_mode) {
                fprintf (stderr, "\n **** %d files successfully processed ****\n", num_files);
                fflush (stderr);
            }
        }

        free (matches);
    }
    else {
        error_line ("nothing to do!");
        ++error_count;
    }

    if (outfilename)
        free (outfilename);

    if (set_console_title)
        DoSetConsoleTitle ("WvUnpack Completed");

    return error_count ? 1 : 0;
}