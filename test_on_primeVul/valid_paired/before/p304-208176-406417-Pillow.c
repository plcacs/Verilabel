convert(
    Imaging imOut, Imaging imIn, const char *mode, ImagingPalette palette, int dither) {
    ImagingSectionCookie cookie;
    ImagingShuffler convert;
    int y;

    if (!imIn) {
        return (Imaging)ImagingError_ModeError();
    }

    if (!mode) {
        /* Map palette image to full depth */
        if (!imIn->palette) {
            return (Imaging)ImagingError_ModeError();
        }
        mode = imIn->palette->mode;
    } else {
        /* Same mode? */
        if (!strcmp(imIn->mode, mode)) {
            return ImagingCopy2(imOut, imIn);
        }
    }

    /* test for special conversions */

    if (strcmp(imIn->mode, "P") == 0 || strcmp(imIn->mode, "PA") == 0) {
        return frompalette(imOut, imIn, mode);
    }

    if (strcmp(mode, "P") == 0 || strcmp(mode, "PA") == 0) {
        return topalette(imOut, imIn, mode, palette, dither);
    }

    if (dither && strcmp(mode, "1") == 0) {
        return tobilevel(imOut, imIn, dither);
    }

    /* standard conversion machinery */

    convert = NULL;

    for (y = 0; converters[y].from; y++) {
        if (!strcmp(imIn->mode, converters[y].from) &&
            !strcmp(mode, converters[y].to)) {
            convert = converters[y].convert;
            break;
        }
    }

    if (!convert) {
#ifdef notdef
        return (Imaging)ImagingError_ValueError("conversion not supported");
#else
        static char buf[100];
        sprintf(buf, "conversion from %.10s to %.10s not supported", imIn->mode, mode);
        return (Imaging)ImagingError_ValueError(buf);
#endif
    }

    imOut = ImagingNew2Dirty(mode, imOut, imIn);
    if (!imOut) {
        return NULL;
    }

    ImagingSectionEnter(&cookie);
    for (y = 0; y < imIn->ysize; y++) {
        (*convert)((UINT8 *)imOut->image[y], (UINT8 *)imIn->image[y], imIn->xsize);
    }
    ImagingSectionLeave(&cookie);

    return imOut;
}