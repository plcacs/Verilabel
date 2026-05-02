ImagingResampleHorizontal(Imaging imIn, int xsize, int filter)
{
    ImagingSectionCookie cookie;
    Imaging imOut;
    struct filter *filterp;
    float support, scale, filterscale;
    float center, ww, ss, ss0, ss1, ss2, ss3;
    int xx, yy, x, kmax, xmin, xmax;
    int *xbounds;
    float *k, *kk;

    /* check filter */
    switch (filter) {
    case IMAGING_TRANSFORM_LANCZOS:
        filterp = &LANCZOS;
        break;
    case IMAGING_TRANSFORM_BILINEAR:
        filterp = &BILINEAR;
        break;
    case IMAGING_TRANSFORM_BICUBIC:
        filterp = &BICUBIC;
        break;
    default:
        return (Imaging) ImagingError_ValueError(
            "unsupported resampling filter"
            );
    }

    /* prepare for horizontal stretch */
    filterscale = scale = (float) imIn->xsize / xsize;

    /* determine support size (length of resampling filter) */
    support = filterp->support;

    if (filterscale < 1.0) {
        filterscale = 1.0;
    }

    support = support * filterscale;

    /* maximum number of coofs */
    kmax = (int) ceil(support) * 2 + 1;

    /* coefficient buffer */
    kk = malloc(xsize * kmax * sizeof(float));
    if ( ! kk)
        return (Imaging) ImagingError_MemoryError();

    xbounds = malloc(xsize * 2 * sizeof(int));
    if ( ! xbounds) {
        free(kk);
        return (Imaging) ImagingError_MemoryError();
    }

    for (xx = 0; xx < xsize; xx++) {
        k = &kk[xx * kmax];
        center = (xx + 0.5) * scale;
        ww = 0.0;
        ss = 1.0 / filterscale;
        xmin = (int) floor(center - support);
        if (xmin < 0)
            xmin = 0;
        xmax = (int) ceil(center + support);
        if (xmax > imIn->xsize)
            xmax = imIn->xsize;
        for (x = xmin; x < xmax; x++) {
            float w = filterp->filter((x - center + 0.5) * ss) * ss;
            k[x - xmin] = w;
            ww += w;
        }
        for (x = 0; x < xmax - xmin; x++) {
            if (ww != 0.0)
                k[x] /= ww;
        }
        xbounds[xx * 2 + 0] = xmin;
        xbounds[xx * 2 + 1] = xmax;
    }

    imOut = ImagingNew(imIn->mode, xsize, imIn->ysize);
    if ( ! imOut) {
        free(kk);
        free(xbounds);
        return NULL;
    }

    ImagingSectionEnter(&cookie);
    /* horizontal stretch */
    for (yy = 0; yy < imOut->ysize; yy++) {
        if (imIn->image8) {
            /* 8-bit grayscale */
            for (xx = 0; xx < xsize; xx++) {
                xmin = xbounds[xx * 2 + 0];
                xmax = xbounds[xx * 2 + 1];
                k = &kk[xx * kmax];
                ss = 0.5;
                for (x = xmin; x < xmax; x++)
                    ss += i2f(imIn->image8[yy][x]) * k[x - xmin];
                imOut->image8[yy][xx] = clip8(ss);
            }
        } else {
            switch(imIn->type) {
            case IMAGING_TYPE_UINT8:
                /* n-bit grayscale */
                if (imIn->bands == 2) {
                    for (xx = 0; xx < xsize; xx++) {
                        xmin = xbounds[xx * 2 + 0];
                        xmax = xbounds[xx * 2 + 1];
                        k = &kk[xx * kmax];
                        ss0 = ss1 = 0.5;
                        for (x = xmin; x < xmax; x++) {
                            ss0 += i2f((UINT8) imIn->image[yy][x*4 + 0]) * k[x - xmin];
                            ss1 += i2f((UINT8) imIn->image[yy][x*4 + 3]) * k[x - xmin];
                        }
                        imOut->image[yy][xx*4 + 0] = clip8(ss0);
                        imOut->image[yy][xx*4 + 3] = clip8(ss1);
                    }
                } else if (imIn->bands == 3) {
                    for (xx = 0; xx < xsize; xx++) {
                        xmin = xbounds[xx * 2 + 0];
                        xmax = xbounds[xx * 2 + 1];
                        k = &kk[xx * kmax];
                        ss0 = ss1 = ss2 = 0.5;
                        for (x = xmin; x < xmax; x++) {
                            ss0 += i2f((UINT8) imIn->image[yy][x*4 + 0]) * k[x - xmin];
                            ss1 += i2f((UINT8) imIn->image[yy][x*4 + 1]) * k[x - xmin];
                            ss2 += i2f((UINT8) imIn->image[yy][x*4 + 2]) * k[x - xmin];
                        }
                        imOut->image[yy][xx*4 + 0] = clip8(ss0);
                        imOut->image[yy][xx*4 + 1] = clip8(ss1);
                        imOut->image[yy][xx*4 + 2] = clip8(ss2);
                    }
                } else {
                    for (xx = 0; xx < xsize; xx++) {
                        xmin = xbounds[xx * 2 + 0];
                        xmax = xbounds[xx * 2 + 1];
                        k = &kk[xx * kmax];
                        ss0 = ss1 = ss2 = ss3 = 0.5;
                        for (x = xmin; x < xmax; x++) {
                            ss0 += i2f((UINT8) imIn->image[yy][x*4 + 0]) * k[x - xmin];
                            ss1 += i2f((UINT8) imIn->image[yy][x*4 + 1]) * k[x - xmin];
                            ss2 += i2f((UINT8) imIn->image[yy][x*4 + 2]) * k[x - xmin];
                            ss3 += i2f((UINT8) imIn->image[yy][x*4 + 3]) * k[x - xmin];
                        }
                        imOut->image[yy][xx*4 + 0] = clip8(ss0);
                        imOut->image[yy][xx*4 + 1] = clip8(ss1);
                        imOut->image[yy][xx*4 + 2] = clip8(ss2);
                        imOut->image[yy][xx*4 + 3] = clip8(ss3);
                    }
                }
                break;
            case IMAGING_TYPE_INT32:
                /* 32-bit integer */
                for (xx = 0; xx < xsize; xx++) {
                    xmin = xbounds[xx * 2 + 0];
                    xmax = xbounds[xx * 2 + 1];
                    k = &kk[xx * kmax];
                    ss = 0.0;
                    for (x = xmin; x < xmax; x++)
                        ss += i2f(IMAGING_PIXEL_I(imIn, x, yy)) * k[x - xmin];
                    IMAGING_PIXEL_I(imOut, xx, yy) = (int) ss;
                }
                break;
            case IMAGING_TYPE_FLOAT32:
                /* 32-bit float */
                for (xx = 0; xx < xsize; xx++) {
                    xmin = xbounds[xx * 2 + 0];
                    xmax = xbounds[xx * 2 + 1];
                    k = &kk[xx * kmax];
                    ss = 0.0;
                    for (x = xmin; x < xmax; x++)
                        ss += IMAGING_PIXEL_F(imIn, x, yy) * k[x - xmin];
                    IMAGING_PIXEL_F(imOut, xx, yy) = ss;
                }
                break;
            }
        }
    }
    ImagingSectionLeave(&cookie);
    free(kk);
    free(xbounds);
    return imOut;
}