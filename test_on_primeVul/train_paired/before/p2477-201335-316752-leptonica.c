int main(int    argc,
         char **argv)
{
l_int32       w, h;
PIX          *pixs, *pixg, *pixim, *pixgm, *pixmi, *pix1, *pix2;
PIX          *pixmr, *pixmg, *pixmb, *pixmri, *pixmgi, *pixmbi;
PIXA         *pixa;
L_REGPARAMS  *rp;

    if (regTestSetup(argc, argv, &rp))
        return 1;

    lept_mkdir("lept/adapt");  // REMOVE?

    pixs = pixRead("wet-day.jpg");
    pixa = pixaCreate(0);
    pixg = pixConvertRGBToGray(pixs, 0.33, 0.34, 0.33);
    pixaAddPix(pixa, pixs, L_INSERT);
    pixaAddPix(pixa, pixg, L_INSERT);
    pixGetDimensions(pixs, &w, &h, NULL);

        /* Process in grayscale */
    startTimer();
    pixim = pixCreate(w, h, 1);
    pixRasterop(pixim, XS, YS, WS, HS, PIX_SET, NULL, 0, 0);
    pixGetBackgroundGrayMap(pixg, pixim, SIZE_X, SIZE_Y,
                            BINTHRESH, MINCOUNT, &pixgm);
    fprintf(stderr, "Time for gray adaptmap gen: %7.3f\n", stopTimer());
    regTestWritePixAndCheck(rp, pixgm, IFF_PNG);  /* 0 */
    pixaAddPix(pixa, pixgm, L_INSERT);

    startTimer();
    pixmi = pixGetInvBackgroundMap(pixgm, BGVAL, SMOOTH_X, SMOOTH_Y);
    fprintf(stderr, "Time for gray inv map generation: %7.3f\n", stopTimer());
    regTestWritePixAndCheck(rp, pixmi, IFF_PNG);  /* 1 */
    pixaAddPix(pixa, pixmi, L_INSERT);

    startTimer();
    pix1 = pixApplyInvBackgroundGrayMap(pixg, pixmi, SIZE_X, SIZE_Y);
    fprintf(stderr, "Time to apply gray inv map: %7.3f\n", stopTimer());
    regTestWritePixAndCheck(rp, pix1, IFF_JFIF_JPEG);  /* 2 */
    pixaAddPix(pixa, pix1, L_INSERT);

    pix2 = pixGammaTRCMasked(NULL, pix1, pixim, 1.0, 0, 190);
    pixInvert(pixim, pixim);
    pixGammaTRCMasked(pix2, pix2, pixim, 1.0, 60, 190);
    regTestWritePixAndCheck(rp, pix2, IFF_JFIF_JPEG);  /* 3 */
    pixaAddPix(pixa, pix2, L_INSERT);
    pixDestroy(&pixim);

        /* Process in color */
    startTimer();
    pixim = pixCreate(w, h, 1);
    pixRasterop(pixim, XS, YS, WS, HS, PIX_SET, NULL, 0, 0);
    pixGetBackgroundRGBMap(pixs, pixim, NULL, SIZE_X, SIZE_Y,
                           BINTHRESH, MINCOUNT,
                           &pixmr, &pixmg, &pixmb);
    fprintf(stderr, "Time for color adaptmap gen: %7.3f\n", stopTimer());
    regTestWritePixAndCheck(rp, pixmr, IFF_PNG);  /* 4 */
    regTestWritePixAndCheck(rp, pixmg, IFF_PNG);  /* 5 */
    regTestWritePixAndCheck(rp, pixmb, IFF_PNG);  /* 6 */
    pixaAddPix(pixa, pixmr, L_INSERT);
    pixaAddPix(pixa, pixmg, L_INSERT);
    pixaAddPix(pixa, pixmb, L_INSERT);

    startTimer();
    pixmri = pixGetInvBackgroundMap(pixmr, BGVAL, SMOOTH_X, SMOOTH_Y);
    pixmgi = pixGetInvBackgroundMap(pixmg, BGVAL, SMOOTH_X, SMOOTH_Y);
    pixmbi = pixGetInvBackgroundMap(pixmb, BGVAL, SMOOTH_X, SMOOTH_Y);
    fprintf(stderr, "Time for color inv map generation: %7.3f\n", stopTimer());
    regTestWritePixAndCheck(rp, pixmri, IFF_PNG);  /* 7 */
    regTestWritePixAndCheck(rp, pixmgi, IFF_PNG);  /* 8 */
    regTestWritePixAndCheck(rp, pixmbi, IFF_PNG);  /* 9 */
    pixaAddPix(pixa, pixmri, L_INSERT);
    pixaAddPix(pixa, pixmgi, L_INSERT);
    pixaAddPix(pixa, pixmbi, L_INSERT);

    startTimer();
    pix1 = pixApplyInvBackgroundRGBMap(pixs, pixmri, pixmgi, pixmbi,
                                       SIZE_X, SIZE_Y);
    fprintf(stderr, "Time to apply color inv maps: %7.3f\n", stopTimer());
    regTestWritePixAndCheck(rp, pix1, IFF_JFIF_JPEG);  /* 10 */
    pixaAddPix(pixa, pix1, L_INSERT);

    pix2 = pixGammaTRCMasked(NULL, pix1, pixim, 1.0, 0, 190);
    pixInvert(pixim, pixim);
    pixGammaTRCMasked(pix2, pix2, pixim, 1.0, 60, 190);
    regTestWritePixAndCheck(rp, pix2, IFF_JFIF_JPEG);  /* 11 */
    pixaAddPix(pixa, pix2, L_INSERT);
    pixDestroy(&pixim);

        /* Process at higher level in color */
    startTimer();
    pixim = pixCreate(w, h, 1);
    pixRasterop(pixim, XS, YS, WS, HS, PIX_SET, NULL, 0, 0);
    pix1 = pixBackgroundNorm(pixs, pixim, NULL, 5, 10, BINTHRESH, 20,
                             BGVAL, SMOOTH_X, SMOOTH_Y);
    fprintf(stderr, "Time for bg normalization: %7.3f\n", stopTimer());
    regTestWritePixAndCheck(rp, pix1, IFF_JFIF_JPEG);  /* 12 */
    pixaAddPix(pixa, pix1, L_INSERT);

    pix2 = pixGammaTRCMasked(NULL, pix1, pixim, 1.0, 0, 190);
    pixInvert(pixim, pixim);
    pixGammaTRCMasked(pix2, pix2, pixim, 1.0, 60, 190);
    regTestWritePixAndCheck(rp, pix2, IFF_JFIF_JPEG);  /* 13 */
    pixaAddPix(pixa, pix2, L_INSERT);
    pixDestroy(&pixim);

        /* Display results */
    pix1 = pixaDisplayTiledAndScaled(pixa, 32, 400, 4, 0, 20, 2);
    pixWrite("/tmp/lept/adapt/results.jpg", pix1, IFF_JFIF_JPEG);
    pixDisplayWithTitle(pix1, 100, 0, NULL, rp->display);
    pixDestroy(&pix1);
    pixaDestroy(&pixa);

    return regTestCleanup(rp);
}