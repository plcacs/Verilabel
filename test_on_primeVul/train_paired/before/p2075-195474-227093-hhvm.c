String string_chunk_split(const char *src, int srclen, const char *end,
                          int endlen, int chunklen) {
  int chunks = srclen / chunklen; // complete chunks!
  int restlen = srclen - chunks * chunklen; /* srclen % chunklen */

  int out_len = (chunks + 1) * endlen + srclen;
  String ret(out_len, ReserveString);
  char *dest = ret.bufferSlice().ptr;

  const char *p; char *q;
  const char *pMax = src + srclen - chunklen + 1;
  for (p = src, q = dest; p < pMax; ) {
    memcpy(q, p, chunklen);
    q += chunklen;
    memcpy(q, end, endlen);
    q += endlen;
    p += chunklen;
  }

  if (restlen) {
    memcpy(q, p, restlen);
    q += restlen;
    memcpy(q, end, endlen);
    q += endlen;
  }

  ret.setSize(q - dest);
  return ret;
}