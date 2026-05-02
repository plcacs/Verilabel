static int decomp(unsigned char *srcBuf, unsigned char **jpegBuf,
                  unsigned long *jpegSize, unsigned char *dstBuf, int w, int h,
                  int subsamp, int jpegQual, char *fileName, int tilew,
                  int tileh)
{
  char tempStr[1024], sizeStr[24] = "\0", qualStr[13] = "\0", *ptr;
  FILE *file = NULL;
  tjhandle handle = NULL;
  int row, col, iter = 0, dstBufAlloc = 0, retval = 0;
  double elapsed, elapsedDecode;
  int ps = tjPixelSize[pf];
  int scaledw = TJSCALED(w, sf);
  int scaledh = TJSCALED(h, sf);
  int pitch = scaledw * ps;
  int ntilesw = (w + tilew - 1) / tilew, ntilesh = (h + tileh - 1) / tileh;
  unsigned char *dstPtr, *dstPtr2, *yuvBuf = NULL;

  if (jpegQual > 0) {
    snprintf(qualStr, 13, "_Q%d", jpegQual);
    qualStr[12] = 0;
  }

  if ((handle = tjInitDecompress()) == NULL)
    THROW_TJ("executing tjInitDecompress()");

  if (dstBuf == NULL) {
    if ((unsigned long long)pitch * (unsigned long long)scaledh >
        (unsigned long long)((size_t)-1))
      THROW("allocating destination buffer", "Image is too large");
    if ((dstBuf = (unsigned char *)malloc((size_t)pitch * scaledh)) == NULL)
      THROW_UNIX("allocating destination buffer");
    dstBufAlloc = 1;
  }
  /* Set the destination buffer to gray so we know whether the decompressor
     attempted to write to it */
  memset(dstBuf, 127, pitch * scaledh);

  if (doYUV) {
    int width = doTile ? tilew : scaledw;
    int height = doTile ? tileh : scaledh;
    unsigned long yuvSize = tjBufSizeYUV2(width, yuvPad, height, subsamp);

    if (yuvSize == (unsigned long)-1)
      THROW_TJ("allocating YUV buffer");
    if ((yuvBuf = (unsigned char *)malloc(yuvSize)) == NULL)
      THROW_UNIX("allocating YUV buffer");
    memset(yuvBuf, 127, yuvSize);
  }

  /* Benchmark */
  iter = -1;
  elapsed = elapsedDecode = 0.;
  while (1) {
    int tile = 0;
    double start = getTime();

    for (row = 0, dstPtr = dstBuf; row < ntilesh;
         row++, dstPtr += pitch * tileh) {
      for (col = 0, dstPtr2 = dstPtr; col < ntilesw;
           col++, tile++, dstPtr2 += ps * tilew) {
        int width = doTile ? min(tilew, w - col * tilew) : scaledw;
        int height = doTile ? min(tileh, h - row * tileh) : scaledh;

        if (doYUV) {
          double startDecode;

          if (tjDecompressToYUV2(handle, jpegBuf[tile], jpegSize[tile], yuvBuf,
                                 width, yuvPad, height, flags) == -1)
            THROW_TJ("executing tjDecompressToYUV2()");
          startDecode = getTime();
          if (tjDecodeYUV(handle, yuvBuf, yuvPad, subsamp, dstPtr2, width,
                          pitch, height, pf, flags) == -1)
            THROW_TJ("executing tjDecodeYUV()");
          if (iter >= 0) elapsedDecode += getTime() - startDecode;
        } else if (tjDecompress2(handle, jpegBuf[tile], jpegSize[tile],
                                 dstPtr2, width, pitch, height, pf,
                                 flags) == -1)
          THROW_TJ("executing tjDecompress2()");
      }
    }
    elapsed += getTime() - start;
    if (iter >= 0) {
      iter++;
      if (elapsed >= benchTime) break;
    } else if (elapsed >= warmup) {
      iter = 0;
      elapsed = elapsedDecode = 0.;
    }
  }
  if (doYUV) elapsed -= elapsedDecode;

  if (tjDestroy(handle) == -1) THROW_TJ("executing tjDestroy()");
  handle = NULL;

  if (quiet) {
    printf("%-6s%s",
           sigfig((double)(w * h) / 1000000. * (double)iter / elapsed, 4,
                  tempStr, 1024),
           quiet == 2 ? "\n" : "  ");
    if (doYUV)
      printf("%s\n",
             sigfig((double)(w * h) / 1000000. * (double)iter / elapsedDecode,
                    4, tempStr, 1024));
    else if (quiet != 2) printf("\n");
  } else {
    printf("%s --> Frame rate:         %f fps\n",
           doYUV ? "Decomp to YUV" : "Decompress   ", (double)iter / elapsed);
    printf("                  Throughput:         %f Megapixels/sec\n",
           (double)(w * h) / 1000000. * (double)iter / elapsed);
    if (doYUV) {
      printf("YUV Decode    --> Frame rate:         %f fps\n",
             (double)iter / elapsedDecode);
      printf("                  Throughput:         %f Megapixels/sec\n",
             (double)(w * h) / 1000000. * (double)iter / elapsedDecode);
    }
  }

  if (!doWrite) goto bailout;

  if (sf.num != 1 || sf.denom != 1)
    snprintf(sizeStr, 24, "%d_%d", sf.num, sf.denom);
  else if (tilew != w || tileh != h)
    snprintf(sizeStr, 24, "%dx%d", tilew, tileh);
  else snprintf(sizeStr, 24, "full");
  if (decompOnly)
    snprintf(tempStr, 1024, "%s_%s.%s", fileName, sizeStr, ext);
  else
    snprintf(tempStr, 1024, "%s_%s%s_%s.%s", fileName, subName[subsamp],
             qualStr, sizeStr, ext);

  if (tjSaveImage(tempStr, dstBuf, scaledw, 0, scaledh, pf, flags) == -1)
    THROW_TJG("saving bitmap");
  ptr = strrchr(tempStr, '.');
  snprintf(ptr, 1024 - (ptr - tempStr), "-err.%s", ext);
  if (srcBuf && sf.num == 1 && sf.denom == 1) {
    if (!quiet) printf("Compression error written to %s.\n", tempStr);
    if (subsamp == TJ_GRAYSCALE) {
      unsigned long index, index2;

      for (row = 0, index = 0; row < h; row++, index += pitch) {
        for (col = 0, index2 = index; col < w; col++, index2 += ps) {
          unsigned long rindex = index2 + tjRedOffset[pf];
          unsigned long gindex = index2 + tjGreenOffset[pf];
          unsigned long bindex = index2 + tjBlueOffset[pf];
          int y = (int)((double)srcBuf[rindex] * 0.299 +
                        (double)srcBuf[gindex] * 0.587 +
                        (double)srcBuf[bindex] * 0.114 + 0.5);

          if (y > 255) y = 255;
          if (y < 0) y = 0;
          dstBuf[rindex] = abs(dstBuf[rindex] - y);
          dstBuf[gindex] = abs(dstBuf[gindex] - y);
          dstBuf[bindex] = abs(dstBuf[bindex] - y);
        }
      }
    } else {
      for (row = 0; row < h; row++)
        for (col = 0; col < w * ps; col++)
          dstBuf[pitch * row + col] =
            abs(dstBuf[pitch * row + col] - srcBuf[pitch * row + col]);
    }
    if (tjSaveImage(tempStr, dstBuf, w, 0, h, pf, flags) == -1)
      THROW_TJG("saving bitmap");
  }

bailout:
  if (file) fclose(file);
  if (handle) tjDestroy(handle);
  if (dstBuf && dstBufAlloc) free(dstBuf);
  if (yuvBuf) free(yuvBuf);
  return retval;
}