static mf_t *open_mf_pattern(void *talloc_ctx, struct demuxer *d, char *filename)
{
    struct mp_log *log = d->log;
    int error_count = 0;
    int count = 0;

    mf_t *mf = talloc_zero(talloc_ctx, mf_t);
    mf->log = log;

    if (filename[0] == '@') {
        struct stream *s = stream_create(filename + 1,
                            d->stream_origin | STREAM_READ, d->cancel, d->global);
        if (s) {
            while (1) {
                char buf[512];
                int len = stream_read_peek(s, buf, sizeof(buf));
                if (!len)
                    break;
                bstr data = (bstr){buf, len};
                int pos = bstrchr(data, '\n');
                data = bstr_splice(data, 0, pos < 0 ? data.len : pos + 1);
                bstr fname = bstr_strip(data);
                if (fname.len) {
                    if (bstrchr(fname, '\0') >= 0) {
                        mp_err(log, "invalid filename\n");
                        break;
                    }
                    char *entry = bstrto0(mf, fname);
                    if (!mp_path_exists(entry)) {
                        mp_verbose(log, "file not found: '%s'\n", entry);
                    } else {
                        MP_TARRAY_APPEND(mf, mf->names, mf->nr_of_files, entry);
                    }
                }
                stream_seek_skip(s, stream_tell(s) + data.len);
            }
            free_stream(s);

            mp_info(log, "number of files: %d\n", mf->nr_of_files);
            goto exit_mf;
        }
        mp_info(log, "%s is not indirect filelist\n", filename + 1);
    }

    if (strchr(filename, ',')) {
        mp_info(log, "filelist: %s\n", filename);
        bstr bfilename = bstr0(filename);

        while (bfilename.len) {
            bstr bfname;
            bstr_split_tok(bfilename, ",", &bfname, &bfilename);
            char *fname2 = bstrdup0(mf, bfname);

            if (!mp_path_exists(fname2))
                mp_verbose(log, "file not found: '%s'\n", fname2);
            else {
                mf_add(mf, fname2);
            }
            talloc_free(fname2);
        }
        mp_info(log, "number of files: %d\n", mf->nr_of_files);

        goto exit_mf;
    }

    size_t fname_avail = strlen(filename) + 32;
    char *fname = talloc_size(mf, fname_avail);

#if HAVE_GLOB
    if (!strchr(filename, '%')) {
        strcpy(fname, filename);
        if (!strchr(filename, '*'))
            strcat(fname, "*");

        mp_info(log, "search expr: %s\n", fname);

        glob_t gg;
        if (glob(fname, 0, NULL, &gg)) {
            talloc_free(mf);
            return NULL;
        }

        for (int i = 0; i < gg.gl_pathc; i++) {
            if (mp_path_isdir(gg.gl_pathv[i]))
                continue;
            mf_add(mf, gg.gl_pathv[i]);
        }
        mp_info(log, "number of files: %d\n", mf->nr_of_files);
        globfree(&gg);
        goto exit_mf;
    }
#endif

    // We're using arbitrary user input as printf format with 1 int argument.
    // Any format which uses exactly 1 int argument would be valid, but for
    // simplicity we reject all conversion specifiers except %% and simple
    // integer specifier: %[.][NUM]d where NUM is 1-3 digits (%.d is valid)
    const char *f = filename;
    int MAXDIGS = 3, nspec = 0, bad_spec = 0, c;

    while (nspec < 2 && (c = *f++)) {
        if (c != '%')
            continue;
        if (*f != '%') {
            nspec++;  // conversion specifier which isn't %%
            if (*f == '.')
                f++;
            for (int ndig = 0; mp_isdigit(*f) && ndig < MAXDIGS; ndig++, f++)
                /* no-op */;
            if (*f != 'd') {
                bad_spec++;  // not int, or beyond our validation capacity
                break;
            }
        }
        // *f is '%' or 'd'
        f++;
    }

    // nspec==0 (zero specifiers) is rejected because fname wouldn't advance.
    if (bad_spec || nspec != 1) {
        mp_err(log, "unsupported expr format: '%s'\n", filename);
        goto exit_mf;
    }

    mp_info(log, "search expr: %s\n", filename);

    while (error_count < 5) {
        if (snprintf(fname, fname_avail, filename, count++) >= fname_avail) {
            mp_err(log, "format result too long: '%s'\n", filename);
            goto exit_mf;
        }
        if (!mp_path_exists(fname)) {
            error_count++;
            mp_verbose(log, "file not found: '%s'\n", fname);
        } else {
            mf_add(mf, fname);
        }
    }

    mp_info(log, "number of files: %d\n", mf->nr_of_files);

exit_mf:
    return mf;
}