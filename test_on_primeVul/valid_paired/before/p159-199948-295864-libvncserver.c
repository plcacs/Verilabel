rfbSendRectEncodingRaw(rfbClientPtr cl,
                       int x,
                       int y,
                       int w,
                       int h)
{
    rfbFramebufferUpdateRectHeader rect;
    int nlines;
    int bytesPerLine = w * (cl->format.bitsPerPixel / 8);
    char *fbptr = (cl->scaledScreen->frameBuffer + (cl->scaledScreen->paddedWidthInBytes * y)
                   + (x * (cl->scaledScreen->bitsPerPixel / 8)));

    /* Flush the buffer to guarantee correct alignment for translateFn(). */
    if (cl->ublen > 0) {
        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingRaw);

    memcpy(&cl->updateBuf[cl->ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
    cl->ublen += sz_rfbFramebufferUpdateRectHeader;


    rfbStatRecordEncodingSent(cl, rfbEncodingRaw, sz_rfbFramebufferUpdateRectHeader + bytesPerLine * h,
        sz_rfbFramebufferUpdateRectHeader + bytesPerLine * h);

    nlines = (UPDATE_BUF_SIZE - cl->ublen) / bytesPerLine;

    while (TRUE) {
        if (nlines > h)
            nlines = h;

        (*cl->translateFn)(cl->translateLookupTable,
			   &(cl->screen->serverFormat),
                           &cl->format, fbptr, &cl->updateBuf[cl->ublen],
                           cl->scaledScreen->paddedWidthInBytes, w, nlines);

        cl->ublen += nlines * bytesPerLine;
        h -= nlines;

        if (h == 0)     /* rect fitted in buffer, do next one */
            return TRUE;

        /* buffer full - flush partial rect and do another nlines */

        if (!rfbSendUpdateBuf(cl))
            return FALSE;

        fbptr += (cl->scaledScreen->paddedWidthInBytes * nlines);

        nlines = (UPDATE_BUF_SIZE - cl->ublen) / bytesPerLine;
        if (nlines == 0) {
            rfbErr("rfbSendRectEncodingRaw: send buffer too small for %d "
                   "bytes per line\n", bytesPerLine);
            rfbCloseClient(cl);
            return FALSE;
        }
    }
}