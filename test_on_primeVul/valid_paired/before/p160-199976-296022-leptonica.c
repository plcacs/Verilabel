pixFewColorsOctcubeQuantMixed(PIX       *pixs,
                              l_int32    level,
                              l_int32    darkthresh,
                              l_int32    lightthresh,
                              l_int32    diffthresh,
                              l_float32  minfract,
                              l_int32    maxspan)
{
l_int32    i, j, w, h, wplc, wplm, wpld, ncolors, index;
l_int32    rval, gval, bval, val, minval, maxval;
l_int32   *lut;
l_uint32  *datac, *datam, *datad, *linec, *linem, *lined;
PIX       *pixc, *pixm, *pixg, *pixd;
PIXCMAP   *cmap, *cmapd;

    PROCNAME("pixFewColorsOctcubeQuantMixed");

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", procName, NULL);
    if (level <= 0) level = 3;
    if (level > 6)
        return (PIX *)ERROR_PTR("invalid level", procName, NULL);
    if (darkthresh <= 0) darkthresh = 20;
    if (lightthresh <= 0) lightthresh = 244;
    if (diffthresh <= 0) diffthresh = 20;
    if (minfract <= 0.0) minfract = 0.05;
    if (maxspan <= 2) maxspan = 15;

        /* Start with a simple fixed octcube quantizer. */
    if ((pixc = pixFewColorsOctcubeQuant1(pixs, level)) == NULL)
        return (PIX *)ERROR_PTR("too many colors", procName, NULL);

        /* Identify and save color entries in the colormap.  Set up a LUT
         * that returns -1 for any gray pixel. */
    cmap = pixGetColormap(pixc);
    ncolors = pixcmapGetCount(cmap);
    cmapd = pixcmapCreate(8);
    lut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = 0; i < 256; i++)
        lut[i] = -1;
    for (i = 0, index = 0; i < ncolors; i++) {
        pixcmapGetColor(cmap, i, &rval, &gval, &bval);
        minval = L_MIN(rval, gval);
        minval = L_MIN(minval, bval);
        if (minval > lightthresh)  /* near white */
            continue;
        maxval = L_MAX(rval, gval);
        maxval = L_MAX(maxval, bval);
        if (maxval < darkthresh)  /* near black */
            continue;

            /* Use the max diff between components to test for color */
        if (maxval - minval >= diffthresh) {
            pixcmapAddColor(cmapd, rval, gval, bval);
            lut[i] = index;
            index++;
        }
    }

        /* Generate dest pix with just the color pixels set to their
         * colormap indices.  At the same time, make a 1 bpp mask
         * of the non-color pixels */
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 8);
    pixSetColormap(pixd, cmapd);
    pixm = pixCreate(w, h, 1);
    datac = pixGetData(pixc);
    datam = pixGetData(pixm);
    datad = pixGetData(pixd);
    wplc = pixGetWpl(pixc);
    wplm = pixGetWpl(pixm);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linec = datac + i * wplc;
        linem = datam + i * wplm;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(linec, j);
            if (lut[val] == -1)
                SET_DATA_BIT(linem, j);
            else
                SET_DATA_BYTE(lined, j, lut[val]);
        }
    }

        /* Fill in the gray values.  Use a grayscale version of pixs
         * as input, along with the mask over the actual gray pixels. */
    pixg = pixConvertTo8(pixs, 0);
    pixGrayQuantFromHisto(pixd, pixg, pixm, minfract, maxspan);

    LEPT_FREE(lut);
    pixDestroy(&pixc);
    pixDestroy(&pixm);
    pixDestroy(&pixg);
    return pixd;
}