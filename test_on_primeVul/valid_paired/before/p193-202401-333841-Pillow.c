_decodeStripYCbCr(Imaging im, ImagingCodecState state, TIFF *tiff) {
    // To avoid dealing with YCbCr subsampling, let libtiff handle it
    // Use a TIFFRGBAImage wrapping the tiff image, and let libtiff handle
    // all of the conversion. Metadata read from the TIFFRGBAImage could
    // be different from the metadata that the base tiff returns.

    INT32 strip_row;
    UINT8 *new_data;
    UINT32 rows_per_strip, row_byte_size, rows_to_read;
    int ret;
    TIFFRGBAImage img;
    char emsg[1024] = "";

    ret = TIFFGetFieldDefaulted(tiff, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
    if (ret != 1) {
        rows_per_strip = state->ysize;
    }
    TRACE(("RowsPerStrip: %u \n", rows_per_strip));

    if (!(TIFFRGBAImageOK(tiff, emsg) && TIFFRGBAImageBegin(&img, tiff, 0, emsg))) {
        TRACE(("Decode error, msg: %s", emsg));
        state->errcode = IMAGING_CODEC_BROKEN;
        // nothing to clean up, just return
        return -1;
    }

    img.req_orientation = ORIENTATION_TOPLEFT;
    img.col_offset = 0;

    if (state->xsize != img.width || state->ysize != img.height) {
        TRACE(
            ("Inconsistent Image Error: %d =? %d, %d =? %d",
             state->xsize,
             img.width,
             state->ysize,
             img.height));
        state->errcode = IMAGING_CODEC_BROKEN;
        goto decodeycbcr_err;
    }

    /* overflow check for row byte size */
    if (INT_MAX / 4 < img.width) {
        state->errcode = IMAGING_CODEC_MEMORY;
        goto decodeycbcr_err;
    }

    // TiffRGBAImages are 32bits/pixel.
    row_byte_size = img.width * 4;

    /* overflow check for realloc */
    if (INT_MAX / row_byte_size < rows_per_strip) {
        state->errcode = IMAGING_CODEC_MEMORY;
        goto decodeycbcr_err;
    }

    state->bytes = rows_per_strip * row_byte_size;

    TRACE(("StripSize: %d \n", state->bytes));

    /* realloc to fit whole strip */
    /* malloc check above */
    new_data = realloc(state->buffer, state->bytes);
    if (!new_data) {
        state->errcode = IMAGING_CODEC_MEMORY;
        goto decodeycbcr_err;
    }

    state->buffer = new_data;

    for (; state->y < state->ysize; state->y += rows_per_strip) {
        img.row_offset = state->y;
        rows_to_read = min(rows_per_strip, img.height - state->y);

        if (TIFFRGBAImageGet(&img, (UINT32 *)state->buffer, img.width, rows_to_read) ==
            -1) {
            TRACE(("Decode Error, y: %d\n", state->y));
            state->errcode = IMAGING_CODEC_BROKEN;
            goto decodeycbcr_err;
        }

        TRACE(("Decoded strip for row %d \n", state->y));

        // iterate over each row in the strip and stuff data into image
        for (strip_row = 0;
             strip_row < min((INT32)rows_per_strip, state->ysize - state->y);
             strip_row++) {
            TRACE(("Writing data into line %d ; \n", state->y + strip_row));

            // UINT8 * bbb = state->buffer + strip_row * (state->bytes /
            // rows_per_strip); TRACE(("chars: %x %x %x %x\n", ((UINT8 *)bbb)[0],
            // ((UINT8 *)bbb)[1], ((UINT8 *)bbb)[2], ((UINT8 *)bbb)[3]));

            state->shuffle(
                (UINT8 *)im->image[state->y + state->yoff + strip_row] +
                    state->xoff * im->pixelsize,
                state->buffer + strip_row * row_byte_size,
                state->xsize);
        }
    }

decodeycbcr_err:
    TIFFRGBAImageEnd(&img);
    if (state->errcode != 0) {
        return -1;
    }
    return 0;
}