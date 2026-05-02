DLLEXPORT unsigned char *tjLoadImage(const char *filename, int *width,
                                     int align, int *height, int *pixelFormat,
                                     int flags)
{
  int retval = 0, tempc, pitch;
  tjhandle handle = NULL;
  tjinstance *this;
  j_compress_ptr cinfo = NULL;
  cjpeg_source_ptr src;
  unsigned char *dstBuf = NULL;
  FILE *file = NULL;
  boolean invert;

  if (!filename || !width || align < 1 || !height || !pixelFormat ||
      *pixelFormat < TJPF_UNKNOWN || *pixelFormat >= TJ_NUMPF)
    _throwg("tjLoadImage(): Invalid argument");
  if ((align & (align - 1)) != 0)
    _throwg("tjLoadImage(): Alignment must be a power of 2");

  if ((handle = tjInitCompress()) == NULL) return NULL;
  this = (tjinstance *)handle;
  cinfo = &this->cinfo;

  if ((file = fopen(filename, "rb")) == NULL)
    _throwunix("tjLoadImage(): Cannot open input file");

  if ((tempc = getc(file)) < 0 || ungetc(tempc, file) == EOF)
    _throwunix("tjLoadImage(): Could not read input file")
  else if (tempc == EOF)
    _throwg("tjLoadImage(): Input file contains no data");

  if (setjmp(this->jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error. */
    retval = -1;  goto bailout;
  }

  if (*pixelFormat == TJPF_UNKNOWN) cinfo->in_color_space = JCS_UNKNOWN;
  else cinfo->in_color_space = pf2cs[*pixelFormat];
  if (tempc == 'B') {
    if ((src = jinit_read_bmp(cinfo, FALSE)) == NULL)
      _throwg("tjLoadImage(): Could not initialize bitmap loader");
    invert = (flags & TJFLAG_BOTTOMUP) == 0;
  } else if (tempc == 'P') {
    if ((src = jinit_read_ppm(cinfo)) == NULL)
      _throwg("tjLoadImage(): Could not initialize bitmap loader");
    invert = (flags & TJFLAG_BOTTOMUP) != 0;
  } else
    _throwg("tjLoadImage(): Unsupported file type");

  src->input_file = file;
  (*src->start_input) (cinfo, src);
  (*cinfo->mem->realize_virt_arrays) ((j_common_ptr)cinfo);

  *width = cinfo->image_width;  *height = cinfo->image_height;
  *pixelFormat = cs2pf[cinfo->in_color_space];

  pitch = PAD((*width) * tjPixelSize[*pixelFormat], align);
  if ((dstBuf = (unsigned char *)malloc(pitch * (*height))) == NULL)
    _throwg("tjLoadImage(): Memory allocation failure");

  if (setjmp(this->jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error. */
    retval = -1;  goto bailout;
  }

  while (cinfo->next_scanline < cinfo->image_height) {
    int i, nlines = (*src->get_pixel_rows) (cinfo, src);

    for (i = 0; i < nlines; i++) {
      unsigned char *dstptr;
      int row;

      row = cinfo->next_scanline + i;
      if (invert) dstptr = &dstBuf[((*height) - row - 1) * pitch];
      else dstptr = &dstBuf[row * pitch];
      memcpy(dstptr, src->buffer[i], (*width) * tjPixelSize[*pixelFormat]);
    }
    cinfo->next_scanline += nlines;
  }

  (*src->finish_input) (cinfo, src);

bailout:
  if (handle) tjDestroy(handle);
  if (file) fclose(file);
  if (retval < 0 && dstBuf) { free(dstBuf);  dstBuf = NULL; }
  return dstBuf;
}