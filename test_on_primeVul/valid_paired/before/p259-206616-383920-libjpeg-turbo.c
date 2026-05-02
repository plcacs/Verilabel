start_input_gif(j_compress_ptr cinfo, cjpeg_source_ptr sinfo)
{
  gif_source_ptr source = (gif_source_ptr)sinfo;
  U_CHAR hdrbuf[10];            /* workspace for reading control blocks */
  unsigned int width, height;   /* image dimensions */
  int colormaplen, aspectRatio;
  int c;

  /* Read and verify GIF Header */
  if (!ReadOK(source->pub.input_file, hdrbuf, 6))
    ERREXIT(cinfo, JERR_GIF_NOT);
  if (hdrbuf[0] != 'G' || hdrbuf[1] != 'I' || hdrbuf[2] != 'F')
    ERREXIT(cinfo, JERR_GIF_NOT);
  /* Check for expected version numbers.
   * If unknown version, give warning and try to process anyway;
   * this is per recommendation in GIF89a standard.
   */
  if ((hdrbuf[3] != '8' || hdrbuf[4] != '7' || hdrbuf[5] != 'a') &&
      (hdrbuf[3] != '8' || hdrbuf[4] != '9' || hdrbuf[5] != 'a'))
    TRACEMS3(cinfo, 1, JTRC_GIF_BADVERSION, hdrbuf[3], hdrbuf[4], hdrbuf[5]);

  /* Read and decipher Logical Screen Descriptor */
  if (!ReadOK(source->pub.input_file, hdrbuf, 7))
    ERREXIT(cinfo, JERR_INPUT_EOF);
  width = LM_to_uint(hdrbuf, 0);
  height = LM_to_uint(hdrbuf, 2);
  /* we ignore the color resolution, sort flag, and background color index */
  aspectRatio = UCH(hdrbuf[6]);
  if (aspectRatio != 0 && aspectRatio != 49)
    TRACEMS(cinfo, 1, JTRC_GIF_NONSQUARE);

  /* Allocate space to store the colormap */
  source->colormap = (*cinfo->mem->alloc_sarray)
    ((j_common_ptr)cinfo, JPOOL_IMAGE, (JDIMENSION)MAXCOLORMAPSIZE,
     (JDIMENSION)NUMCOLORS);
  colormaplen = 0;              /* indicate initialization */

  /* Read global colormap if header indicates it is present */
  if (BitSet(hdrbuf[4], COLORMAPFLAG)) {
    colormaplen = 2 << (hdrbuf[4] & 0x07);
    ReadColorMap(source, colormaplen, source->colormap);
  }

  /* Scan until we reach start of desired image.
   * We don't currently support skipping images, but could add it easily.
   */
  for (;;) {
    c = ReadByte(source);

    if (c == ';')               /* GIF terminator?? */
      ERREXIT(cinfo, JERR_GIF_IMAGENOTFOUND);

    if (c == '!') {             /* Extension */
      DoExtension(source);
      continue;
    }

    if (c != ',') {             /* Not an image separator? */
      WARNMS1(cinfo, JWRN_GIF_CHAR, c);
      continue;
    }

    /* Read and decipher Local Image Descriptor */
    if (!ReadOK(source->pub.input_file, hdrbuf, 9))
      ERREXIT(cinfo, JERR_INPUT_EOF);
    /* we ignore top/left position info, also sort flag */
    width = LM_to_uint(hdrbuf, 4);
    height = LM_to_uint(hdrbuf, 6);
    source->is_interlaced = (BitSet(hdrbuf[8], INTERLACE) != 0);

    /* Read local colormap if header indicates it is present */
    /* Note: if we wanted to support skipping images, */
    /* we'd need to skip rather than read colormap for ignored images */
    if (BitSet(hdrbuf[8], COLORMAPFLAG)) {
      colormaplen = 2 << (hdrbuf[8] & 0x07);
      ReadColorMap(source, colormaplen, source->colormap);
    }

    source->input_code_size = ReadByte(source); /* get min-code-size byte */
    if (source->input_code_size < 2 || source->input_code_size > 8)
      ERREXIT1(cinfo, JERR_GIF_CODESIZE, source->input_code_size);

    /* Reached desired image, so break out of loop */
    /* If we wanted to skip this image, */
    /* we'd call SkipDataBlocks and then continue the loop */
    break;
  }

  /* Prepare to read selected image: first initialize LZW decompressor */
  source->symbol_head = (UINT16 *)
    (*cinfo->mem->alloc_large) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                LZW_TABLE_SIZE * sizeof(UINT16));
  source->symbol_tail = (UINT8 *)
    (*cinfo->mem->alloc_large) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                LZW_TABLE_SIZE * sizeof(UINT8));
  source->symbol_stack = (UINT8 *)
    (*cinfo->mem->alloc_large) ((j_common_ptr)cinfo, JPOOL_IMAGE,
                                LZW_TABLE_SIZE * sizeof(UINT8));
  InitLZWCode(source);

  /*
   * If image is interlaced, we read it into a full-size sample array,
   * decompressing as we go; then get_interlaced_row selects rows from the
   * sample array in the proper order.
   */
  if (source->is_interlaced) {
    /* We request the virtual array now, but can't access it until virtual
     * arrays have been allocated.  Hence, the actual work of reading the
     * image is postponed until the first call to get_pixel_rows.
     */
    source->interlaced_image = (*cinfo->mem->request_virt_sarray)
      ((j_common_ptr)cinfo, JPOOL_IMAGE, FALSE,
       (JDIMENSION)width, (JDIMENSION)height, (JDIMENSION)1);
    if (cinfo->progress != NULL) {
      cd_progress_ptr progress = (cd_progress_ptr)cinfo->progress;
      progress->total_extra_passes++; /* count file input as separate pass */
    }
    source->pub.get_pixel_rows = load_interlaced_image;
  } else {
    source->pub.get_pixel_rows = get_pixel_rows;
  }

  /* Create compressor input buffer. */
  source->pub.buffer = (*cinfo->mem->alloc_sarray)
    ((j_common_ptr)cinfo, JPOOL_IMAGE, (JDIMENSION)width * NUMCOLORS,
     (JDIMENSION)1);
  source->pub.buffer_height = 1;

  /* Pad colormap for safety. */
  for (c = colormaplen; c < source->clear_code; c++) {
    source->colormap[CM_RED][c]   =
    source->colormap[CM_GREEN][c] =
    source->colormap[CM_BLUE][c]  = CENTERJSAMPLE;
  }

  /* Return info about the image. */
  cinfo->in_color_space = JCS_RGB;
  cinfo->input_components = NUMCOLORS;
  cinfo->data_precision = BITS_IN_JSAMPLE; /* we always rescale data to this */
  cinfo->image_width = width;
  cinfo->image_height = height;

  TRACEMS3(cinfo, 1, JTRC_GIF, width, height, colormaplen);
}