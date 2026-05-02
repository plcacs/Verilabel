main(int argc, char* argv[])
{
	uint16 bitspersample, shortv;
	uint32 imagewidth, imagelength;
	uint16 config = PLANARCONFIG_CONTIG;
	uint32 rowsperstrip = (uint32) -1;
	uint16 photometric = PHOTOMETRIC_RGB;
	uint16 *rmap, *gmap, *bmap;
	uint32 row;
	int cmap = -1;
	TIFF *in, *out;
	int c;

#if !HAVE_DECL_OPTARG
	extern int optind;
	extern char* optarg;
#endif

	while ((c = getopt(argc, argv, "C:c:p:r:")) != -1)
		switch (c) {
		case 'C':		/* force colormap interpretation */
			cmap = atoi(optarg);
			break;
		case 'c':		/* compression scheme */
			if (!processCompressOptions(optarg))
				usage();
			break;
		case 'p':		/* planar configuration */
			if (streq(optarg, "separate"))
				config = PLANARCONFIG_SEPARATE;
			else if (streq(optarg, "contig"))
				config = PLANARCONFIG_CONTIG;
			else
				usage();
			break;
		case 'r':		/* rows/strip */
			rowsperstrip = atoi(optarg);
			break;
		case '?':
			usage();
			/*NOTREACHED*/
		}
	if (argc - optind != 2)
		usage();
	in = TIFFOpen(argv[optind], "r");
	if (in == NULL)
		return (-1);
	if (!TIFFGetField(in, TIFFTAG_PHOTOMETRIC, &shortv) ||
	    shortv != PHOTOMETRIC_PALETTE) {
		fprintf(stderr, "%s: Expecting a palette image.\n",
		    argv[optind]);
		return (-1);
	}
	if (!TIFFGetField(in, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap)) {
		fprintf(stderr,
		    "%s: No colormap (not a valid palette image).\n",
		    argv[optind]);
		return (-1);
	}
	bitspersample = 0;
	TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	if (bitspersample != 8) {
		fprintf(stderr, "%s: Sorry, can only handle 8-bit images.\n",
		    argv[optind]);
		return (-1);
	}
	out = TIFFOpen(argv[optind+1], "w");
	if (out == NULL)
		return (-2);
	cpTags(in, out);
	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &imagewidth);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &imagelength);
	if (compression != (uint16)-1)
		TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
	else
		TIFFGetField(in, TIFFTAG_COMPRESSION, &compression);
	switch (compression) {
	case COMPRESSION_JPEG:
		if (jpegcolormode == JPEGCOLORMODE_RGB)
			photometric = PHOTOMETRIC_YCBCR;
		else
			photometric = PHOTOMETRIC_RGB;
		TIFFSetField(out, TIFFTAG_JPEGQUALITY, quality);
		TIFFSetField(out, TIFFTAG_JPEGCOLORMODE, jpegcolormode);
		break;
	case COMPRESSION_LZW:
	case COMPRESSION_DEFLATE:
		if (predictor != 0)
			TIFFSetField(out, TIFFTAG_PREDICTOR, predictor);
		break;
	}
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, photometric);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 3);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, config);
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP,
	    rowsperstrip = TIFFDefaultStripSize(out, rowsperstrip));
	(void) TIFFGetField(in, TIFFTAG_PLANARCONFIG, &shortv);
	if (cmap == -1)
		cmap = checkcmap(1<<bitspersample, rmap, gmap, bmap);
	if (cmap == 16) {
		/*
		 * Convert 16-bit colormap to 8-bit.
		 */
		int i;

		for (i = (1<<bitspersample)-1; i >= 0; i--) {
#define	CVT(x)		(((x) * 255) / ((1L<<16)-1))
			rmap[i] = CVT(rmap[i]);
			gmap[i] = CVT(gmap[i]);
			bmap[i] = CVT(bmap[i]);
		}
	}
	{ unsigned char *ibuf, *obuf;
	  register unsigned char* pp;
	  register uint32 x;
	  tmsize_t tss_in = TIFFScanlineSize(in);
	  tmsize_t tss_out = TIFFScanlineSize(out);
	  if (tss_out / tss_in < 3) {
		/*
		 * BUG 2750: The following code does not know about chroma
		 * subsampling of JPEG data. It assumes that the output buffer is 3x
		 * the length of the input buffer due to exploding the palette into
		 * RGB tuples. If this assumption is incorrect, it could lead to a
		 * buffer overflow. Go ahead and fail now to prevent that.
		 */
		fprintf(stderr, "Could not determine correct image size for output. Exiting.\n");
		return -1;
      }
	  ibuf = (unsigned char*)_TIFFmalloc(tss_in);
	  obuf = (unsigned char*)_TIFFmalloc(tss_out);
	  switch (config) {
	  case PLANARCONFIG_CONTIG:
		for (row = 0; row < imagelength; row++) {
			if (!TIFFReadScanline(in, ibuf, row, 0))
				goto done;
			pp = obuf;
			for (x = 0; x < imagewidth; x++) {
				*pp++ = (unsigned char) rmap[ibuf[x]];
				*pp++ = (unsigned char) gmap[ibuf[x]];
				*pp++ = (unsigned char) bmap[ibuf[x]];
			}
			if (!TIFFWriteScanline(out, obuf, row, 0))
				goto done;
		}
		break;
	  case PLANARCONFIG_SEPARATE:
		for (row = 0; row < imagelength; row++) {
			if (!TIFFReadScanline(in, ibuf, row, 0))
				goto done;
			for (pp = obuf, x = 0; x < imagewidth; x++)
				*pp++ = (unsigned char) rmap[ibuf[x]];
			if (!TIFFWriteScanline(out, obuf, row, 0))
				goto done;
			for (pp = obuf, x = 0; x < imagewidth; x++)
				*pp++ = (unsigned char) gmap[ibuf[x]];
			if (!TIFFWriteScanline(out, obuf, row, 0))
				goto done;
			for (pp = obuf, x = 0; x < imagewidth; x++)
				*pp++ = (unsigned char) bmap[ibuf[x]];
			if (!TIFFWriteScanline(out, obuf, row, 0))
				goto done;
		}
		break;
	  }
	  _TIFFfree(ibuf);
	  _TIFFfree(obuf);
	}
done:
	(void) TIFFClose(in);
	(void) TIFFClose(out);
	return (0);
}