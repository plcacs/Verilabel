pixFillMapHoles(PIX     *pix,
                l_int32  nx,
                l_int32  ny,
                l_int32  filltype)
{
l_int32   w, h, y, nmiss, goodcol, i, j, found, ival, valtest;
l_uint32  val, lastval;
NUMA     *na;  /* indicates if there is any data in the column */
PIX      *pixt;

    PROCNAME("pixFillMapHoles");

    if (!pix || pixGetDepth(pix) != 8)
        return ERROR_INT("pix not defined or not 8 bpp", procName, 1);
    if (pixGetColormap(pix))
        return ERROR_INT("pix is colormapped", procName, 1);

    /* ------------- Fill holes in the mapping image columns ----------- */
    pixGetDimensions(pix, &w, &h, NULL);
    na = numaCreate(0);  /* holds flag for which columns have data */
    nmiss = 0;
    valtest = (filltype == L_FILL_WHITE) ? 255 : 0;
    for (j = 0; j < nx; j++) {  /* do it by columns */
        found = FALSE;
        for (i = 0; i < ny; i++) {
            pixGetPixel(pix, j, i, &val);
            if (val != valtest) {
                y = i;
                found = TRUE;
                break;
            }
        }
        if (found == FALSE) {
            numaAddNumber(na, 0);  /* no data in the column */
            nmiss++;
        }
        else {
            numaAddNumber(na, 1);  /* data in the column */
            for (i = y - 1; i >= 0; i--)  /* replicate upwards to top */
                pixSetPixel(pix, j, i, val);
            pixGetPixel(pix, j, 0, &lastval);
            for (i = 1; i < h; i++) {  /* set going down to bottom */
                pixGetPixel(pix, j, i, &val);
                if (val == valtest)
                    pixSetPixel(pix, j, i, lastval);
                else
                    lastval = val;
            }
        }
    }
    numaAddNumber(na, 0);  /* last column */

    if (nmiss == nx) {  /* no data in any column! */
        numaDestroy(&na);
        L_WARNING("no bg found; no data in any column\n", procName);
        return 1;
    }

    /* ---------- Fill in missing columns by replication ----------- */
    if (nmiss > 0) {  /* replicate columns */
        pixt = pixCopy(NULL, pix);
            /* Find the first good column */
        goodcol = 0;
        for (j = 0; j < w; j++) {
            numaGetIValue(na, j, &ival);
            if (ival == 1) {
                goodcol = j;
                break;
            }
        }
        if (goodcol > 0) {  /* copy cols backward */
            for (j = goodcol - 1; j >= 0; j--) {
                pixRasterop(pix, j, 0, 1, h, PIX_SRC, pixt, j + 1, 0);
                pixRasterop(pixt, j, 0, 1, h, PIX_SRC, pix, j, 0);
            }
        }
        for (j = goodcol + 1; j < w; j++) {   /* copy cols forward */
            numaGetIValue(na, j, &ival);
            if (ival == 0) {
                    /* Copy the column to the left of j */
                pixRasterop(pix, j, 0, 1, h, PIX_SRC, pixt, j - 1, 0);
                pixRasterop(pixt, j, 0, 1, h, PIX_SRC, pix, j, 0);
            }
        }
        pixDestroy(&pixt);
    }
    if (w > nx) {  /* replicate the last column */
        for (i = 0; i < h; i++) {
            pixGetPixel(pix, w - 2, i, &val);
            pixSetPixel(pix, w - 1, i, val);
        }
    }

    numaDestroy(&na);
    return 0;
}