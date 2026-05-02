int ImagingLibTiffDecode(Imaging im, ImagingCodecState state, UINT8* buffer, Py_ssize_t bytes) {
    TIFFSTATE *clientstate = (TIFFSTATE *)state->context;
    char *filename = "tempfile.tif";
    char *mode = "r";
    TIFF *tiff;

    /* buffer is the encoded file, bytes is the length of the encoded file */
    /*     it all ends up in state->buffer, which is a uint8* from Imaging.h */

    TRACE(("in decoder: bytes %d\n", bytes));
    TRACE(("State: count %d, state %d, x %d, y %d, ystep %d\n", state->count, state->state,
           state->x, state->y, state->ystep));
    TRACE(("State: xsize %d, ysize %d, xoff %d, yoff %d \n", state->xsize, state->ysize,
           state->xoff, state->yoff));
    TRACE(("State: bits %d, bytes %d \n", state->bits, state->bytes));
    TRACE(("Buffer: %p: %c%c%c%c\n", buffer, (char)buffer[0], (char)buffer[1],(char)buffer[2], (char)buffer[3]));
    TRACE(("State->Buffer: %c%c%c%c\n", (char)state->buffer[0], (char)state->buffer[1],(char)state->buffer[2], (char)state->buffer[3]));
    TRACE(("Image: mode %s, type %d, bands: %d, xsize %d, ysize %d \n",
           im->mode, im->type, im->bands, im->xsize, im->ysize));
    TRACE(("Image: image8 %p, image32 %p, image %p, block %p \n",
           im->image8, im->image32, im->image, im->block));
    TRACE(("Image: pixelsize: %d, linesize %d \n",
           im->pixelsize, im->linesize));

    dump_state(clientstate);
    clientstate->size = bytes;
    clientstate->eof = clientstate->size;
    clientstate->loc = 0;
    clientstate->data = (tdata_t)buffer;
    clientstate->flrealloc = 0;
    dump_state(clientstate);

    TIFFSetWarningHandler(NULL);
    TIFFSetWarningHandlerExt(NULL);

    if (clientstate->fp) {
        TRACE(("Opening using fd: %d\n",clientstate->fp));
        lseek(clientstate->fp,0,SEEK_SET); // Sometimes, I get it set to the end.
        tiff = TIFFFdOpen(clientstate->fp, filename, mode);
    } else {
        TRACE(("Opening from string\n"));
        tiff = TIFFClientOpen(filename, mode,
                              (thandle_t) clientstate,
                              _tiffReadProc, _tiffWriteProc,
                              _tiffSeekProc, _tiffCloseProc, _tiffSizeProc,
                              _tiffMapProc, _tiffUnmapProc);
    }

    if (!tiff){
        TRACE(("Error, didn't get the tiff\n"));
        state->errcode = IMAGING_CODEC_BROKEN;
        return -1;
    }

    if (clientstate->ifd){
        int rv;
        uint32 ifdoffset = clientstate->ifd;
        TRACE(("reading tiff ifd %u\n", ifdoffset));
        rv = TIFFSetSubDirectory(tiff, ifdoffset);
        if (!rv){
            TRACE(("error in TIFFSetSubDirectory"));
            return -1;
        }
    }

    if (TIFFIsTiled(tiff)) {
        UINT32 x, y, tile_y, row_byte_size;
        UINT32 tile_width, tile_length, current_tile_width;
        UINT8 *new_data;

        TIFFGetField(tiff, TIFFTAG_TILEWIDTH, &tile_width);
        TIFFGetField(tiff, TIFFTAG_TILELENGTH, &tile_length);

        // We could use TIFFTileSize, but for YCbCr data it returns subsampled data size
        row_byte_size = (tile_width * state->bits + 7) / 8;
        state->bytes = row_byte_size * tile_length;

        /* overflow check for malloc */
        if (state->bytes > INT_MAX - 1) {
            state->errcode = IMAGING_CODEC_MEMORY;
            TIFFClose(tiff);
            return -1;
        }

        /* realloc to fit whole tile */
        new_data = realloc (state->buffer, state->bytes);
        if (!new_data) {
            state->errcode = IMAGING_CODEC_MEMORY;
            TIFFClose(tiff);
            return -1;
        }

        state->buffer = new_data;

        TRACE(("TIFFTileSize: %d\n", state->bytes));

        for (y = state->yoff; y < state->ysize; y += tile_length) {
            for (x = state->xoff; x < state->xsize; x += tile_width) {
                if (ReadTile(tiff, x, y, (UINT32*) state->buffer) == -1) {
                    TRACE(("Decode Error, Tile at %dx%d\n", x, y));
                    state->errcode = IMAGING_CODEC_BROKEN;
                    TIFFClose(tiff);
                    return -1;
                }

                TRACE(("Read tile at %dx%d; \n\n", x, y));

                current_tile_width = min(tile_width, state->xsize - x);

                // iterate over each line in the tile and stuff data into image
                for (tile_y = 0; tile_y < min(tile_length, state->ysize - y); tile_y++) {
                    TRACE(("Writing tile data at %dx%d using tile_width: %d; \n", tile_y + y, x, current_tile_width));

                    // UINT8 * bbb = state->buffer + tile_y * row_byte_size;
                    // TRACE(("chars: %x%x%x%x\n", ((UINT8 *)bbb)[0], ((UINT8 *)bbb)[1], ((UINT8 *)bbb)[2], ((UINT8 *)bbb)[3]));

                    state->shuffle((UINT8*) im->image[tile_y + y] + x * im->pixelsize,
                       state->buffer + tile_y * row_byte_size,
                       current_tile_width
                    );
                }
            }
        }
    } else {
        UINT32 strip_row, row_byte_size;
        UINT8 *new_data;
        UINT32 rows_per_strip;
        int ret;

        ret = TIFFGetField(tiff, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
        if (ret != 1) {
            rows_per_strip = state->ysize;
        }
        TRACE(("RowsPerStrip: %u \n", rows_per_strip));

        // We could use TIFFStripSize, but for YCbCr data it returns subsampled data size
        row_byte_size = (state->xsize * state->bits + 7) / 8;
        state->bytes = rows_per_strip * row_byte_size;

        TRACE(("StripSize: %d \n", state->bytes));

        /* realloc to fit whole strip */
        new_data = realloc (state->buffer, state->bytes);
        if (!new_data) {
            state->errcode = IMAGING_CODEC_MEMORY;
            TIFFClose(tiff);
            return -1;
        }

        state->buffer = new_data;

        for (; state->y < state->ysize; state->y += rows_per_strip) {
            if (ReadStrip(tiff, state->y, (UINT32 *)state->buffer) == -1) {
                TRACE(("Decode Error, strip %d\n", TIFFComputeStrip(tiff, state->y, 0)));
                state->errcode = IMAGING_CODEC_BROKEN;
                TIFFClose(tiff);
                return -1;
            }

            TRACE(("Decoded strip for row %d \n", state->y));

            // iterate over each row in the strip and stuff data into image
            for (strip_row = 0; strip_row < min(rows_per_strip, state->ysize - state->y); strip_row++) {
                TRACE(("Writing data into line %d ; \n", state->y + strip_row));

                // UINT8 * bbb = state->buffer + strip_row * (state->bytes / rows_per_strip);
                // TRACE(("chars: %x %x %x %x\n", ((UINT8 *)bbb)[0], ((UINT8 *)bbb)[1], ((UINT8 *)bbb)[2], ((UINT8 *)bbb)[3]));

                state->shuffle((UINT8*) im->image[state->y + state->yoff + strip_row] +
                               state->xoff * im->pixelsize,
                               state->buffer + strip_row * row_byte_size,
                               state->xsize);
            }
        }
    }

    TIFFClose(tiff);
    TRACE(("Done Decoding, Returning \n"));
    // Returning -1 here to force ImageFile.load to break, rather than
    // even think about looping back around.
    return -1;
}