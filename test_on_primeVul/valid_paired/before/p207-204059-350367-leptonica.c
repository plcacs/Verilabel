pixReadFromTiffStream(TIFF  *tif)
{
char      *text;
l_uint8   *linebuf, *data, *rowptr;
l_uint16   spp, bps, photometry, tiffcomp, orientation, sample_fmt;
l_uint16  *redmap, *greenmap, *bluemap;
l_int32    d, wpl, bpl, comptype, i, j, k, ncolors, rval, gval, bval, aval;
l_int32    xres, yres, tiffbpl, packedbpl, halfsize;
l_uint32   w, h, tiffword, read_oriented;
l_uint32  *line, *ppixel, *tiffdata, *pixdata;
PIX       *pix, *pix1;
PIXCMAP   *cmap;

    PROCNAME("pixReadFromTiffStream");

    if (!tif)
        return (PIX *)ERROR_PTR("tif not defined", procName, NULL);

    read_oriented = 0;

        /* Only accept uint image data:
         *   SAMPLEFORMAT_UINT = 1;
         *   SAMPLEFORMAT_INT = 2;
         *   SAMPLEFORMAT_IEEEFP = 3;
         *   SAMPLEFORMAT_VOID = 4;   */
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sample_fmt);
    if (sample_fmt != SAMPLEFORMAT_UINT) {
        L_ERROR("sample format = %d is not uint\n", procName, sample_fmt);
        return NULL;
    }

        /* Can't read tiff in tiled format. For what is involved, see, e.g:
         *   https://www.cs.rochester.edu/~nelson/courses/vision/\
         *     resources/tiff/libtiff.html#Tiles
         * A tiled tiff can be converted to a normal (strip) tif:
         *   tiffcp -s <input-tiled-tif> <output-strip-tif>    */
    if (TIFFIsTiled(tif)) {
        L_ERROR("tiled format is not supported\n", procName);
        return NULL;
    }

        /* Old style jpeg is not supported.  We tried supporting 8 bpp.
         * TIFFReadScanline() fails on this format, so we used RGBA
         * reading, which generates a 4 spp image, and pulled out the
         * red component.  However, there were problems with double-frees
         * in cleanup.  For RGB, tiffbpl is exactly half the size that
         * you would expect for the raster data in a scanline, which
         * is 3 * w.  */
    TIFFGetFieldDefaulted(tif, TIFFTAG_COMPRESSION, &tiffcomp);
    if (tiffcomp == COMPRESSION_OJPEG) {
        L_ERROR("old style jpeg format is not supported\n", procName);
        return NULL;
    }

        /* Use default fields for bps and spp */
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bps);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);
    if (bps != 1 && bps != 2 && bps != 4 && bps != 8 && bps != 16) {
        L_ERROR("invalid bps = %d\n", procName, bps);
        return NULL;
    }
    if (spp == 2 && bps != 8) {
        L_WARNING("for 2 spp, only handle 8 bps\n", procName);
        return NULL;
    }
    if (spp == 1)
        d = bps;
    else if (spp == 2)  /* gray plus alpha */
        d = 32;  /* will convert to RGBA */
    else if (spp == 3 || spp == 4)
        d = 32;
    else
        return (PIX *)ERROR_PTR("spp not in set {1,2,3,4}", procName, NULL);

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    if (w > MaxTiffWidth) {
        L_ERROR("width = %d pixels; too large\n", procName, w);
        return NULL;
    }
    if (h > MaxTiffHeight) {
        L_ERROR("height = %d pixels; too large\n", procName, h);
        return NULL;
    }

        /* The relation between the size of a byte buffer required to hold
           a raster of image pixels (packedbpl) and the size of the tiff
           buffer (tiffbuf) is either 1:1 or approximately 2:1, depending
           on how the data is stored and subsampled.  Allow some slop
           when validating the relation between buffer size and the image
           parameters w, spp and bps. */
    tiffbpl = TIFFScanlineSize(tif);
    packedbpl = (bps * spp * w + 7) / 8;
    halfsize = L_ABS(2 * tiffbpl - packedbpl) <= 8;
#if 0
    if (halfsize)
        L_INFO("packedbpl = %d is approx. twice tiffbpl = %d\n", procName,
               packedbpl, tiffbpl);
#endif
    if (tiffbpl != packedbpl && !halfsize) {
        L_ERROR("invalid tiffbpl: tiffbpl = %d, packedbpl = %d, "
                "bps = %d, spp = %d, w = %d\n",
                procName, tiffbpl, packedbpl, bps, spp, w);
        return NULL;
    }

    if ((pix = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pix not made", procName, NULL);
    pixSetInputFormat(pix, IFF_TIFF);
    data = (l_uint8 *)pixGetData(pix);
    wpl = pixGetWpl(pix);
    bpl = 4 * wpl;

    if (spp == 1) {
        linebuf = (l_uint8 *)LEPT_CALLOC(tiffbpl + 1, sizeof(l_uint8));
        for (i = 0; i < h; i++) {
            if (TIFFReadScanline(tif, linebuf, i, 0) < 0) {
                LEPT_FREE(linebuf);
                pixDestroy(&pix);
                return (PIX *)ERROR_PTR("line read fail", procName, NULL);
            }
            memcpy(data, linebuf, tiffbpl);
            data += bpl;
        }
        if (bps <= 8)
            pixEndianByteSwap(pix);
        else   /* bps == 16 */
            pixEndianTwoByteSwap(pix);
        LEPT_FREE(linebuf);
    } else if (spp == 2 && bps == 8) {  /* gray plus alpha */
        L_INFO("gray+alpha is not supported; converting to RGBA\n", procName);
        pixSetSpp(pix, 4);
        linebuf = (l_uint8 *)LEPT_CALLOC(tiffbpl + 1, sizeof(l_uint8));
        pixdata = pixGetData(pix);
        for (i = 0; i < h; i++) {
            if (TIFFReadScanline(tif, linebuf, i, 0) < 0) {
                LEPT_FREE(linebuf);
                pixDestroy(&pix);
                return (PIX *)ERROR_PTR("line read fail", procName, NULL);
            }
            rowptr = linebuf;
            ppixel = pixdata + i * wpl;
            for (j = k = 0; j < w; j++) {
                    /* Copy gray value into r, g and b */
                SET_DATA_BYTE(ppixel, COLOR_RED, rowptr[k]);
                SET_DATA_BYTE(ppixel, COLOR_GREEN, rowptr[k]);
                SET_DATA_BYTE(ppixel, COLOR_BLUE, rowptr[k++]);
                SET_DATA_BYTE(ppixel, L_ALPHA_CHANNEL, rowptr[k++]);
                ppixel++;
            }
        }
        LEPT_FREE(linebuf);
    } else {  /* rgb and rgba */
        if ((tiffdata = (l_uint32 *)LEPT_CALLOC((size_t)w * h,
                                                 sizeof(l_uint32))) == NULL) {
            pixDestroy(&pix);
            return (PIX *)ERROR_PTR("calloc fail for tiffdata", procName, NULL);
        }
            /* TIFFReadRGBAImageOriented() converts to 8 bps */
        if (!TIFFReadRGBAImageOriented(tif, w, h, tiffdata,
                                       ORIENTATION_TOPLEFT, 0)) {
            LEPT_FREE(tiffdata);
            pixDestroy(&pix);
            return (PIX *)ERROR_PTR("failed to read tiffdata", procName, NULL);
        } else {
            read_oriented = 1;
        }

        if (spp == 4) pixSetSpp(pix, 4);
        line = pixGetData(pix);
        for (i = 0; i < h; i++, line += wpl) {
            for (j = 0, ppixel = line; j < w; j++) {
                    /* TIFFGet* are macros */
                tiffword = tiffdata[i * w + j];
                rval = TIFFGetR(tiffword);
                gval = TIFFGetG(tiffword);
                bval = TIFFGetB(tiffword);
                if (spp == 3) {
                    composeRGBPixel(rval, gval, bval, ppixel);
                } else {  /* spp == 4 */
                    aval = TIFFGetA(tiffword);
                    composeRGBAPixel(rval, gval, bval, aval, ppixel);
                }
                ppixel++;
            }
        }
        LEPT_FREE(tiffdata);
    }

    if (getTiffStreamResolution(tif, &xres, &yres) == 0) {
        pixSetXRes(pix, xres);
        pixSetYRes(pix, yres);
    }

        /* Find and save the compression type */
    comptype = getTiffCompressedFormat(tiffcomp);
    pixSetInputFormat(pix, comptype);

    if (TIFFGetField(tif, TIFFTAG_COLORMAP, &redmap, &greenmap, &bluemap)) {
            /* Save the colormap as a pix cmap.  Because the
             * tiff colormap components are 16 bit unsigned,
             * and go from black (0) to white (0xffff), the
             * the pix cmap takes the most significant byte. */
        if (bps > 8) {
            pixDestroy(&pix);
            return (PIX *)ERROR_PTR("colormap size > 256", procName, NULL);
        }
        if ((cmap = pixcmapCreate(bps)) == NULL) {
            pixDestroy(&pix);
            return (PIX *)ERROR_PTR("colormap not made", procName, NULL);
        }
        ncolors = 1 << bps;
        for (i = 0; i < ncolors; i++)
            pixcmapAddColor(cmap, redmap[i] >> 8, greenmap[i] >> 8,
                            bluemap[i] >> 8);
        if (pixSetColormap(pix, cmap)) {
            pixDestroy(&pix);
            return (PIX *)ERROR_PTR("invalid colormap", procName, NULL);
        }

            /* Remove the colormap for 1 bpp. */
        if (bps == 1) {
            pix1 = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
            pixDestroy(&pix);
            pix = pix1;
        }
    } else {   /* No colormap: check photometry and invert if necessary */
        if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometry)) {
                /* Guess default photometry setting.  Assume min_is_white
                 * if compressed 1 bpp; min_is_black otherwise. */
            if (tiffcomp == COMPRESSION_CCITTFAX3 ||
                tiffcomp == COMPRESSION_CCITTFAX4 ||
                tiffcomp == COMPRESSION_CCITTRLE ||
                tiffcomp == COMPRESSION_CCITTRLEW) {
                photometry = PHOTOMETRIC_MINISWHITE;
            } else {
                photometry = PHOTOMETRIC_MINISBLACK;
            }
        }
        if ((d == 1 && photometry == PHOTOMETRIC_MINISBLACK) ||
            (d == 8 && photometry == PHOTOMETRIC_MINISWHITE))
            pixInvert(pix, pix);
    }

    if (TIFFGetField(tif, TIFFTAG_ORIENTATION, &orientation)) {
        if (orientation >= 1 && orientation <= 8) {
            struct tiff_transform *transform = (read_oriented) ?
                &tiff_partial_orientation_transforms[orientation - 1] :
                &tiff_orientation_transforms[orientation - 1];
            if (transform->vflip) pixFlipTB(pix, pix);
            if (transform->hflip) pixFlipLR(pix, pix);
            if (transform->rotate) {
                PIX *oldpix = pix;
                pix = pixRotate90(oldpix, transform->rotate);
                pixDestroy(&oldpix);
            }
        }
    }

    text = NULL;
    TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &text);
    if (text) pixSetText(pix, text);
    return pix;
}