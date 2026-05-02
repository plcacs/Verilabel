static bool w2p(char *ip, char *op) {
  FILE *fp = openr(ip);
  if(!fp) return 1;
  bool openwdone = 0;
  uint8_t *x = 0, *b = 0;
  png_struct *p = 0;
  png_info *n = 0;
  uint8_t i[12];
  char *k[] = {"Out of memory", "Broken config, file a bug report",
    "Invalid WebP", "???", "???", "???", "I/O error"};
  // unsupported feature, suspended, canceled
  if(!fread(i, 12, 1, fp)) {
    PF("ERROR reading %s: %s", IP, k[6]);
    goto w2p_close;
  }
  if(memcmp(i, (char[4]){"RIFF"}, 4) || memcmp(i + 8, (char[4]){"WEBP"}, 4)) {
    PF("ERROR reading %s: %s", IP, k[2]);
    goto w2p_close;
  }
  size_t l = ((uint32_t)(i[4] | (i[5] << 8) | (i[6] << 16) | (i[7] << 24))) + 8;
  // ^ RIFF header size
  x = malloc(l);
  if(!x) {
    PF("ERROR reading %s: %s", IP, *k);
    goto w2p_close;
  }
  memcpy(x, i, 12); // should optimize out
  if(!fread(x + 12, l - 12, 1, fp)) {
    PF("ERROR reading %s: %s", IP, k[6]);
    goto w2p_close;
  }
  fclose(fp);
#if defined LOSSYISERROR || defined NOTHREADS
  WebPBitstreamFeatures I;
#else
  WebPDecoderConfig c = {.options.use_threads = 1};
#define I c.input
#endif
  VP8StatusCode r = WebPGetFeatures(x, l, &I);
  if(r) {
    PF("ERROR reading %s: %s", IP, k[r - 1]);
    goto w2p_free;
  }
#define V I.format
#define W ((uint32_t)I.width)
#define H ((uint32_t)I.height)
#define A I.has_alpha
#ifdef LOSSYISERROR
#define FMTSTR
#define FMTARG
#define ANMSTR "%s"
#define ANMARG , "animat"
#else
  char *f[] = {"undefined/mixed", "lossy", "lossless"};
#define FMTSTR "\nFormat: %s"
#define FMTARG , f[V]
#define ANMSTR "animat"
#define ANMARG
#endif
  PFV("Info: %s:\nDimensions: %" PRIu32 " x %" PRIu32
      "\nSize: %zu bytes (%.15g bpp)\nUses alpha: %s" FMTSTR,
    IP, W, H, l, (double)l * 8 / (W * H), A ? "yes" : "no" FMTARG);
  if(I.has_animation) {
    PF("ERROR reading %s: Unsupported feature: " ANMSTR "ion", IP ANMARG);
    goto w2p_free;
  }
#ifdef LOSSYISERROR
  if(V != 2) {
    PF("ERROR reading %s: Unsupported feature: %sion", IP, "lossy compress");
    goto w2p_free;
  }
#endif
#define B ((unsigned)(3 + A))
  b = malloc(W * H * B);
  if(!b) {
    PF("ERROR reading %s: %s", IP, *k);
    goto w2p_free;
  }
#if defined LOSSYISERROR || defined NOTHREADS
  if(!(A ? WebPDecodeRGBAInto : WebPDecodeRGBInto)(
       x, l, b, W * H * B, (int)(W * B))) {
    PF("ERROR reading %s: %s", IP, k[2]);
    goto w2p_free;
  }
#else
  c.output.colorspace = A ? MODE_RGBA : MODE_RGB;
  c.output.is_external_memory = 1;
#define D c.output.u.RGBA
  D.rgba = b;
  D.stride = (int)(W * B);
  D.size = W * H * B;
  r = WebPDecode(x, l, &c);
  if(r) {
    PF("ERROR reading %s: %s", IP, k[r - 1]);
    goto w2p_free;
  }
#endif
  free(x);
  x = 0;
  if(!(fp = openw(op))) goto w2p_free;
  openwdone = !!op;
  p = png_create_write_struct(PNG_LIBPNG_VER_STRING, op, pngwerr, pngwarn);
  if(!p) {
    PF("ERROR writing %s: %s", OP, *k);
    goto w2p_close;
  }
  n = png_create_info_struct(p);
  if(!n) {
    PF("ERROR writing %s: %s", OP, *k);
    goto w2p_close;
  }
  if(setjmp(png_jmpbuf(p))) {
  w2p_close:
    fclose(fp);
  w2p_free:
    if(openwdone) remove(op);
    free(x);
    free(b);
    png_destroy_write_struct(&p, &n);
    return 1;
  }
  pnglen = 0;
  png_set_write_fn(p, fp, pngwrite, pngflush);
  png_set_filter(p, 0, PNG_ALL_FILTERS);
  png_set_compression_level(p, 9);
  // png_set_compression_memlevel(p, 9);
  png_set_IHDR(p, n, W, H, 8, A ? 6 : 2, 0, 0, 0);
  png_write_info(p, n);
  uint8_t *w = b;
  for(unsigned y = H; y; y--) {
    png_write_row(p, w);
    w += W * B;
  }
  png_write_end(p, n);
  png_destroy_write_struct(&p, &n);
  p = 0;
  n = 0;
  free(b);
  b = 0;
  if(fclose(fp)) {
    PF("ERROR closing %s: %s", OP, strerror(errno));
    goto w2p_free;
  }
  PFV("Info: %s:\nDimensions: %" PRIu32 " x %" PRIu32
      "\nSize: %zu bytes (%.15g bpp)\nFormat: %u-bit %s%s%s\nGamma: %.5g",
    OP, W, H, pnglen, (double)pnglen * 8 / (W * H), 8, A ? "RGBA" : "RGB", "",
    "", 1 / 2.2);
  return 0;
}