void TightDecoder::FilterGradient(const rdr::U8* inbuf,
                                  const PixelFormat& pf, PIXEL_T* outbuf,
                                  int stride, const Rect& r)
{
  int x, y, c;
  static rdr::U8 prevRow[TIGHT_MAX_WIDTH*3];
  static rdr::U8 thisRow[TIGHT_MAX_WIDTH*3];
  rdr::U8 pix[3]; 
  int est[3]; 

  memset(prevRow, 0, sizeof(prevRow));

  // Set up shortcut variables
  int rectHeight = r.height();
  int rectWidth = r.width();

  for (y = 0; y < rectHeight; y++) {
    for (x = 0; x < rectWidth; x++) {
      /* First pixel in a row */
      if (x == 0) {
        pf.rgbFromBuffer(pix, &inbuf[y*rectWidth], 1);
        for (c = 0; c < 3; c++)
          pix[c] += prevRow[c];

        memcpy(thisRow, pix, sizeof(pix));

        pf.bufferFromRGB((rdr::U8*)&outbuf[y*stride], pix, 1);

        continue;
      }

      for (c = 0; c < 3; c++) {
        est[c] = prevRow[x*3+c] + pix[c] - prevRow[(x-1)*3+c];
        if (est[c] > 255) {
          est[c] = 255;
        } else if (est[c] < 0) {
          est[c] = 0;
        }
      }

      pf.rgbFromBuffer(pix, &inbuf[y*rectWidth+x], 1);
      for (c = 0; c < 3; c++)
        pix[c] += est[c];

      memcpy(&thisRow[x*3], pix, sizeof(pix));

      pf.bufferFromRGB((rdr::U8*)&outbuf[y*stride+x], pix, 1);
    }

    memcpy(prevRow, thisRow, sizeof(prevRow));
  }
}