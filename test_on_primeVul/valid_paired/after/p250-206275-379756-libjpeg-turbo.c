get_word_rgb_row(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
/* This version is for reading raw-word-format PPM files with any maxval */
{
  ppm_source_ptr source = (ppm_source_ptr)sinfo;
  register JSAMPROW ptr;
  register U_CHAR *bufferptr;
  register JSAMPLE *rescale = source->rescale;
  JDIMENSION col;
  unsigned int maxval = source->maxval;
  register int rindex = rgb_red[cinfo->in_color_space];
  register int gindex = rgb_green[cinfo->in_color_space];
  register int bindex = rgb_blue[cinfo->in_color_space];
  register int aindex = alpha_index[cinfo->in_color_space];
  register int ps = rgb_pixelsize[cinfo->in_color_space];

  if (!ReadOK(source->pub.input_file, source->iobuffer, source->buffer_width))
    ERREXIT(cinfo, JERR_INPUT_EOF);
  ptr = source->pub.buffer[0];
  bufferptr = source->iobuffer;
  for (col = cinfo->image_width; col > 0; col--) {
    register unsigned int temp;
    temp  = UCH(*bufferptr++) << 8;
    temp |= UCH(*bufferptr++);
    if (temp > maxval)
      ERREXIT(cinfo, JERR_PPM_OUTOFRANGE);
    ptr[rindex] = rescale[temp];
    temp  = UCH(*bufferptr++) << 8;
    temp |= UCH(*bufferptr++);
    if (temp > maxval)
      ERREXIT(cinfo, JERR_PPM_OUTOFRANGE);
    ptr[gindex] = rescale[temp];
    temp  = UCH(*bufferptr++) << 8;
    temp |= UCH(*bufferptr++);
    if (temp > maxval)
      ERREXIT(cinfo, JERR_PPM_OUTOFRANGE);
    ptr[bindex] = rescale[temp];
    if (aindex >= 0)
      ptr[aindex] = 0xFF;
    ptr += ps;
  }
  return 1;
}