int ImagingLibTiffDecode(Imaging im, ImagingCodecState state, UINT8* buffer, int bytes) {
	TIFFSTATE *clientstate = (TIFFSTATE *)state->context;
	char *filename = "tempfile.tif";
	char *mode = "r";
	TIFF *tiff;
	int size;


	/* buffer is the encoded file, bytes is the length of the encoded file */
	/* 	it all ends up in state->buffer, which is a uint8* from Imaging.h */

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
		unsigned int ifdoffset = clientstate->ifd;
		TRACE(("reading tiff ifd %d\n", ifdoffset));
		rv = TIFFSetSubDirectory(tiff, ifdoffset);
		if (!rv){
			TRACE(("error in TIFFSetSubDirectory"));
			return -1;
		}
	}

	size = TIFFScanlineSize(tiff);
	TRACE(("ScanlineSize: %d \n", size));
	if (size > state->bytes) {
		TRACE(("Error, scanline size > buffer size\n"));
		state->errcode = IMAGING_CODEC_BROKEN;
		TIFFClose(tiff);
		return -1;
	}

	// Have to do this row by row and shove stuff into the buffer that way,
	// with shuffle.  (or, just alloc a buffer myself, then figure out how to get it
	// back in. Can't use read encoded stripe.

	// This thing pretty much requires that I have the whole image in one shot.
	// Perhaps a stub version would work better???
	while(state->y < state->ysize){
		if (TIFFReadScanline(tiff, (tdata_t)state->buffer, (uint32)state->y, 0) == -1) {
			TRACE(("Decode Error, row %d\n", state->y));
			state->errcode = IMAGING_CODEC_BROKEN;
			TIFFClose(tiff);
			return -1;
		}
		/* TRACE(("Decoded row %d \n", state->y)); */
		state->shuffle((UINT8*) im->image[state->y + state->yoff] +
					       state->xoff * im->pixelsize,
					   state->buffer,
					   state->xsize);

		state->y++;
	}

	TIFFClose(tiff);
	TRACE(("Done Decoding, Returning \n"));
	// Returning -1 here to force ImageFile.load to break, rather than
	// even think about looping back around.
	return -1;
}