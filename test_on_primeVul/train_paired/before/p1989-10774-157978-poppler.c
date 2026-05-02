void Splash::scaleMaskYdXu(SplashImageMaskSource src, void *srcData,
			   int srcWidth, int srcHeight,
			   int scaledWidth, int scaledHeight,
			   SplashBitmap *dest) {
  Guchar *lineBuf;
  Guint *pixBuf;
  Guint pix;
  Guchar *destPtr;
  int yp, yq, xp, xq, yt, y, yStep, xt, x, xStep, d;
  int i, j;

  // Bresenham parameters for y scale
  yp = srcHeight / scaledHeight;
  yq = srcHeight % scaledHeight;

  // Bresenham parameters for x scale
  xp = scaledWidth / srcWidth;
  xq = scaledWidth % srcWidth;

  // allocate buffers
  lineBuf = (Guchar *)gmalloc(srcWidth);
  pixBuf = (Guint *)gmallocn(srcWidth, sizeof(int));

  // init y scale Bresenham
  yt = 0;

  destPtr = dest->data;
  for (y = 0; y < scaledHeight; ++y) {

    // y scale Bresenham
    if ((yt += yq) >= scaledHeight) {
      yt -= scaledHeight;
      yStep = yp + 1;
    } else {
      yStep = yp;
    }

    // read rows from image
    memset(pixBuf, 0, srcWidth * sizeof(int));
    for (i = 0; i < yStep; ++i) {
      (*src)(srcData, lineBuf);
      for (j = 0; j < srcWidth; ++j) {
	pixBuf[j] += lineBuf[j];
      }
    }

    // init x scale Bresenham
    xt = 0;
    d = (255 << 23) / yStep;

    for (x = 0; x < srcWidth; ++x) {

      // x scale Bresenham
      if ((xt += xq) >= srcWidth) {
	xt -= srcWidth;
	xStep = xp + 1;
      } else {
	xStep = xp;
      }

      // compute the final pixel
      pix = pixBuf[x];
      // (255 * pix) / yStep
      pix = (pix * d) >> 23;

      // store the pixel
      for (i = 0; i < xStep; ++i) {
	*destPtr++ = (Guchar)pix;
      }
    }
  }

  gfree(pixBuf);
  gfree(lineBuf);
}