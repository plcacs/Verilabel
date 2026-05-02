gif_process_raster(
    gif_context_t /* in */ *s,
    gif_t         /* in */ *g
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    unsigned char lzw_cs;
    signed int len, code;
    unsigned int first;
    signed int codesize, codemask, avail, oldcode, bits, valid_bits, clear;
    gif_lzw *p;

    lzw_cs = gif_get8(s);
    clear = 1 << lzw_cs;
    first = 1;
    codesize = lzw_cs + 1;
    codemask = (1 << codesize) - 1;
    bits = 0;
    valid_bits = 0;
    for (code = 0; code < clear; code++) {
        g->codes[code].prefix = -1;
        g->codes[code].first = (unsigned char) code;
        g->codes[code].suffix = (unsigned char) code;
    }

    /* support no starting clear code */
    avail = clear + 2;
    oldcode = (-1);

    len = 0;
    for(;;) {
        if (valid_bits < codesize) {
            if (len == 0) {
                len = gif_get8(s); /* start new block */
                if (len == 0) {
                    return SIXEL_OK;
                }
            }
            --len;
            bits |= (signed int) gif_get8(s) << valid_bits;
            valid_bits += 8;
        } else {
            code = bits & codemask;
            bits >>= codesize;
            valid_bits -= codesize;
            /* @OPTIMIZE: is there some way we can accelerate the non-clear path? */
            if (code == clear) {  /* clear code */
                codesize = lzw_cs + 1;
                codemask = (1 << codesize) - 1;
                avail = clear + 2;
                oldcode = -1;
                first = 0;
            } else if (code == clear + 1) { /* end of stream code */
                s->img_buffer += len;
                while ((len = gif_get8(s)) > 0) {
                    s->img_buffer += len;
                }
                return SIXEL_OK;
            } else if (code <= avail) {
                if (first) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: no clear code).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                if (oldcode >= 0) {
                    if (avail < 4096) {
                        p = &g->codes[avail++];
                        p->prefix = (signed short) oldcode;
                        p->first = g->codes[oldcode].first;
                        p->suffix = (code == avail) ? p->first : g->codes[code].first;
                    }
                } else if (code == avail) {
                    sixel_helper_set_additional_message(
                        "corrupt GIF (reason: illegal code in raster).");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }

                gif_out_code(g, (unsigned short) code);

                if ((avail & codemask) == 0 && avail <= 0x0FFF) {
                    codesize++;
                    codemask = (1 << codesize) - 1;
                }

                oldcode = code;
            } else {
                sixel_helper_set_additional_message(
                    "corrupt GIF (reason: illegal code in raster).");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
        }
    }

    status = SIXEL_OK;

end:
    return status;
}