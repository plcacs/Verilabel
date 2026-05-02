xz_decomp(xz_statep state)
{
    int ret;
    unsigned had;
    unsigned long crc, len;
    lzma_stream *strm = &(state->strm);

    lzma_action action = LZMA_RUN;

    /* fill output buffer up to end of deflate stream */
    had = strm->avail_out;
    do {
        /* get more input for inflate() */
        if (strm->avail_in == 0 && xz_avail(state) == -1)
            return -1;
        if (strm->avail_in == 0) {
            xz_error(state, LZMA_DATA_ERROR, "unexpected end of file");
            return -1;
        }
        if (state->eof)
            action = LZMA_FINISH;

        /* decompress and handle errors */
#ifdef LIBXML_ZLIB_ENABLED
        if (state->how == GZIP) {
            state->zstrm.avail_in = (uInt) state->strm.avail_in;
            state->zstrm.next_in = (Bytef *) state->strm.next_in;
            state->zstrm.avail_out = (uInt) state->strm.avail_out;
            state->zstrm.next_out = (Bytef *) state->strm.next_out;
            ret = inflate(&state->zstrm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT) {
                xz_error(state, Z_STREAM_ERROR,
                         "internal error: inflate stream corrupt");
                return -1;
            }
            if (ret == Z_MEM_ERROR)
                ret = LZMA_MEM_ERROR;
            if (ret == Z_DATA_ERROR)
                ret = LZMA_DATA_ERROR;
            if (ret == Z_STREAM_END)
                ret = LZMA_STREAM_END;
            state->strm.avail_in = state->zstrm.avail_in;
            state->strm.next_in = state->zstrm.next_in;
            state->strm.avail_out = state->zstrm.avail_out;
            state->strm.next_out = state->zstrm.next_out;
        } else                  /* state->how == LZMA */
#endif
            ret = lzma_code(strm, action);
        if (ret == LZMA_MEM_ERROR) {
            xz_error(state, LZMA_MEM_ERROR, "out of memory");
            return -1;
        }
        if (ret == LZMA_DATA_ERROR) {
            xz_error(state, LZMA_DATA_ERROR, "compressed data error");
            return -1;
        }
        if (ret == LZMA_PROG_ERROR) {
            xz_error(state, LZMA_PROG_ERROR, "compression error");
            return -1;
        }
    } while (strm->avail_out && ret != LZMA_STREAM_END);

    /* update available output and crc check value */
    state->have = had - strm->avail_out;
    state->next = strm->next_out - state->have;
#ifdef LIBXML_ZLIB_ENABLED
    state->zstrm.adler =
        crc32(state->zstrm.adler, state->next, state->have);
#endif

    if (ret == LZMA_STREAM_END) {
#ifdef LIBXML_ZLIB_ENABLED
        if (state->how == GZIP) {
            if (gz_next4(state, &crc) == -1 || gz_next4(state, &len) == -1) {
                xz_error(state, LZMA_DATA_ERROR, "unexpected end of file");
                return -1;
            }
            if (crc != state->zstrm.adler) {
                xz_error(state, LZMA_DATA_ERROR, "incorrect data check");
                return -1;
            }
            if (len != (state->zstrm.total_out & 0xffffffffL)) {
                xz_error(state, LZMA_DATA_ERROR, "incorrect length check");
                return -1;
            }
            state->strm.avail_in = 0;
            state->strm.next_in = NULL;
            state->strm.avail_out = 0;
            state->strm.next_out = NULL;
        } else
#endif
        if (strm->avail_in != 0 || !state->eof) {
            xz_error(state, LZMA_DATA_ERROR, "trailing garbage");
            return -1;
        }
        state->how = LOOK;      /* ready for next stream, once have is 0 (leave
                                 * state->direct unchanged to remember how) */
    }

    /* good decompression */
    return 0;
}