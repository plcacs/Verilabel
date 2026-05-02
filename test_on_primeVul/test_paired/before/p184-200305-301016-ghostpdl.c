pcx_write_rle(const byte * from, const byte * end, int step, gp_file * file)
{				/*
                                 * The PCX format theoretically allows encoding runs of 63
                                 * identical bytes, but some readers can't handle repetition
                                 * counts greater than 15.
                                 */
#define MAX_RUN_COUNT 15
    int max_run = step * MAX_RUN_COUNT;

    while (from < end) {
        byte data = *from;

        from += step;
        if (data != *from || from == end) {
            if (data >= 0xc0)
                gp_fputc(0xc1, file);
        } else {
            const byte *start = from;

            while ((from < end) && (*from == data))
                from += step;
            /* Now (from - start) / step + 1 is the run length. */
            while (from - start >= max_run) {
                gp_fputc(0xc0 + MAX_RUN_COUNT, file);
                gp_fputc(data, file);
                start += max_run;
            }
            if (from > start || data >= 0xc0)
                gp_fputc((from - start) / step + 0xc1, file);
        }
        gp_fputc(data, file);
    }
#undef MAX_RUN_COUNT
}