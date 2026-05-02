void ZRLE_DECODE (const Rect& r, rdr::InStream* is,
                  rdr::ZlibInStream* zis,
                  const PixelFormat& pf, ModifiablePixelBuffer* pb)
{
  int length = is->readU32();
  zis->setUnderlying(is, length);
  Rect t;
  PIXEL_T buf[64 * 64];

  for (t.tl.y = r.tl.y; t.tl.y < r.br.y; t.tl.y += 64) {

    t.br.y = __rfbmin(r.br.y, t.tl.y + 64);

    for (t.tl.x = r.tl.x; t.tl.x < r.br.x; t.tl.x += 64) {

      t.br.x = __rfbmin(r.br.x, t.tl.x + 64);

      int mode = zis->readU8();
      bool rle = mode & 128;
      int palSize = mode & 127;
      PIXEL_T palette[128];

      for (int i = 0; i < palSize; i++) {
        palette[i] = READ_PIXEL(zis);
      }

      if (palSize == 1) {
        PIXEL_T pix = palette[0];
        pb->fillRect(pf, t, &pix);
        continue;
      }

      if (!rle) {
        if (palSize == 0) {

          // raw

#ifdef CPIXEL
          for (PIXEL_T* ptr = buf; ptr < buf+t.area(); ptr++) {
            *ptr = READ_PIXEL(zis);
          }
#else
          zis->readBytes(buf, t.area() * (BPP / 8));
#endif

        } else {

          // packed pixels
          int bppp = ((palSize > 16) ? 8 :
                      ((palSize > 4) ? 4 : ((palSize > 2) ? 2 : 1)));

          PIXEL_T* ptr = buf;

          for (int i = 0; i < t.height(); i++) {
            PIXEL_T* eol = ptr + t.width();
            rdr::U8 byte = 0;
            rdr::U8 nbits = 0;

            while (ptr < eol) {
              if (nbits == 0) {
                byte = zis->readU8();
                nbits = 8;
              }
              nbits -= bppp;
              rdr::U8 index = (byte >> nbits) & ((1 << bppp) - 1) & 127;
              *ptr++ = palette[index];
            }
          }
        }

      } else {

        if (palSize == 0) {

          // plain RLE

          PIXEL_T* ptr = buf;
          PIXEL_T* end = ptr + t.area();
          while (ptr < end) {
            PIXEL_T pix = READ_PIXEL(zis);
            int len = 1;
            int b;
            do {
              b = zis->readU8();
              len += b;
            } while (b == 255);

            if (end - ptr < len) {
              throw Exception ("ZRLE decode error");
            }

            while (len-- > 0) *ptr++ = pix;

          }
        } else {

          // palette RLE

          PIXEL_T* ptr = buf;
          PIXEL_T* end = ptr + t.area();
          while (ptr < end) {
            int index = zis->readU8();
            int len = 1;
            if (index & 128) {
              int b;
              do {
                b = zis->readU8();
                len += b;
              } while (b == 255);

              if (end - ptr < len) {
                throw Exception ("ZRLE decode error");
              }
            }

            index &= 127;

            PIXEL_T pix = palette[index];

            while (len-- > 0) *ptr++ = pix;
          }
        }
      }

      pb->imageRect(pf, t, buf);
    }
  }

  zis->removeUnderlying();
}