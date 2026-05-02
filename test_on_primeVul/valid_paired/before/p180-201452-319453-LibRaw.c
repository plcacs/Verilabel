int LibRaw::ljpeg_start(struct jhead *jh, int info_only)
{
  ushort c, tag, len;
  int cnt = 0;
  uchar data[0x10000];
  const uchar *dp;

  memset(jh, 0, sizeof *jh);
  jh->restart = INT_MAX;
  if ((fgetc(ifp), fgetc(ifp)) != 0xd8)
    return 0;
  do
  {
    if (feof(ifp))
      return 0;
    if (cnt++ > 1024)
      return 0; // 1024 tags limit
    if (!fread(data, 2, 2, ifp))
      return 0;
    tag = data[0] << 8 | data[1];
    len = (data[2] << 8 | data[3]) - 2;
    if (tag <= 0xff00)
      return 0;
    fread(data, 1, len, ifp);
    switch (tag)
    {
    case 0xffc3: // start of frame; lossless, Huffman
      jh->sraw = ((data[7] >> 4) * (data[7] & 15) - 1) & 3;
    case 0xffc1:
    case 0xffc0:
      jh->algo = tag & 0xff;
      jh->bits = data[0];
      jh->high = data[1] << 8 | data[2];
      jh->wide = data[3] << 8 | data[4];
      jh->clrs = data[5] + jh->sraw;
      if (len == 9 && !dng_version)
        getc(ifp);
      break;
    case 0xffc4: // define Huffman tables
      if (info_only)
        break;
      for (dp = data; dp < data + len && !((c = *dp++) & -20);)
        jh->free[c] = jh->huff[c] = make_decoder_ref(&dp);
      break;
    case 0xffda: // start of scan
      jh->psv = data[1 + data[0] * 2];
      jh->bits -= data[3 + data[0] * 2] & 15;
      break;
    case 0xffdb:
      FORC(64) jh->quant[c] = data[c * 2 + 1] << 8 | data[c * 2 + 2];
      break;
    case 0xffdd:
      jh->restart = data[0] << 8 | data[1];
    }
  } while (tag != 0xffda);
  if (jh->bits > 16 || jh->clrs > 6 || !jh->bits || !jh->high || !jh->wide ||
      !jh->clrs)
    return 0;
  if (info_only)
    return 1;
  if (!jh->huff[0])
    return 0;
  FORC(19) if (!jh->huff[c + 1]) jh->huff[c + 1] = jh->huff[c];
  if (jh->sraw)
  {
    FORC(4) jh->huff[2 + c] = jh->huff[1];
    FORC(jh->sraw) jh->huff[1 + c] = jh->huff[0];
  }
  jh->row = (ushort *)calloc(jh->wide * jh->clrs, 4);
  merror(jh->row, "ljpeg_start()");
  return zero_after_ff = 1;
}