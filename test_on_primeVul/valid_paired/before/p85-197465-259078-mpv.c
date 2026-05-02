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

    char *fname = talloc_size(mf, strlen(filename) + 32);

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

    mp_info(log, "search expr: %s\n", filename);

    while (error_count < 5) {
        sprintf(fname, filename, count++);
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