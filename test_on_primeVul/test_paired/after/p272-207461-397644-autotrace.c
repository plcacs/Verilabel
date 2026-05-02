at_bitmap input_bmp_reader(gchar * filename, at_input_opts_type * opts, at_msg_func msg_func, gpointer msg_data, gpointer user_data)
{
  FILE *fd;
  unsigned char buffer[128];
  int ColormapSize, rowbytes, Maps;
  gboolean Grey = FALSE;
  unsigned char ColorMap[256][3];
  at_bitmap image = at_bitmap_init(0, 0, 0, 1);
  unsigned char *image_storage;
  at_exception_type exp = at_exception_new(msg_func, msg_data);
  char magick[2];
  Bitmap_Channel masks[4];

  fd = fopen(filename, "rb");

  if (!fd) {
    LOG("Can't open \"%s\"\n", filename);
    at_exception_fatal(&exp, "bmp: cannot open input file");
    goto cleanup;
  }

  /* It is a File. Now is it a Bitmap? Read the shortest possible header. */

  if (!ReadOK(fd, magick, 2) ||
	  !(!strncmp(magick, "BA", 2) ||
		  !strncmp(magick, "BM", 2) ||
		  !strncmp(magick, "IC", 2) ||
		  !strncmp(magick, "PT", 2) ||
		  !strncmp(magick, "CI", 2) ||
		  !strncmp(magick, "CP", 2)))
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  while (!strncmp(magick, "BA", 2))
  {
	  if (!ReadOK(fd, buffer, 12))
	  {
		  LOG("%s is not a valid BMP file", filename);
		  at_exception_fatal(&exp, "bmp: invalid input file");
		  goto cleanup;
	  }

	  if (!ReadOK(fd, magick, 2))
	  {
		  LOG("%s is not a valid BMP file", filename);
		  at_exception_fatal(&exp, "bmp: invalid input file");
		  goto cleanup;
	  }
  }

  if (!ReadOK(fd, buffer, 12))////
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  /* bring them to the right byteorder. Not too nice, but it should work */

  Bitmap_File_Head.bfSize = ToL(&buffer[0x00]);
  Bitmap_File_Head.zzHotX = ToS(&buffer[0x04]);
  Bitmap_File_Head.zzHotY = ToS(&buffer[0x06]);
  Bitmap_File_Head.bfOffs = ToL(&buffer[0x08]);

  if (!ReadOK(fd, buffer, 4))
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  Bitmap_File_Head.biSize = ToL(&buffer[0x00]);

  /* What kind of bitmap is it? */

  if (Bitmap_File_Head.biSize == 12) {  /* OS/2 1.x ? */
    if (!ReadOK(fd, buffer, 8)) {
      LOG("Error reading BMP file header\n");
      at_exception_fatal(&exp, "Error reading BMP file header");
      goto cleanup;
    }

    Bitmap_Head.biWidth = ToS(&buffer[0x00]); /* 12 */
    Bitmap_Head.biHeight = ToS(&buffer[0x02]);  /* 14 */
    Bitmap_Head.biPlanes = ToS(&buffer[0x04]);  /* 16 */
    Bitmap_Head.biBitCnt = ToS(&buffer[0x06]);  /* 18 */
    Bitmap_Head.biCompr = 0;
    Bitmap_Head.biSizeIm = 0;
    Bitmap_Head.biXPels = Bitmap_Head.biYPels = 0;
    Bitmap_Head.biClrUsed = 0;
    Bitmap_Head.biClrImp = 0;
    Bitmap_Head.masks[0] = 0;
    Bitmap_Head.masks[1] = 0;
    Bitmap_Head.masks[2] = 0;
    Bitmap_Head.masks[3] = 0;

    memset(masks, 0, sizeof(masks));
    Maps = 3;

  } else if (Bitmap_File_Head.biSize == 40) { /* Windows 3.x */
    if (!ReadOK(fd, buffer, 36))
    {
      LOG ("Error reading BMP file header\n");
      at_exception_fatal(&exp, "Error reading BMP file header");
      goto cleanup;
    }
          

    Bitmap_Head.biWidth = ToL(&buffer[0x00]); /* 12 */
    Bitmap_Head.biHeight = ToL(&buffer[0x04]);  /* 16 */
    Bitmap_Head.biPlanes = ToS(&buffer[0x08]);  /* 1A */
    Bitmap_Head.biBitCnt = ToS(&buffer[0x0A]);  /* 1C */
    Bitmap_Head.biCompr = ToL(&buffer[0x0C]); /* 1E */
    Bitmap_Head.biSizeIm = ToL(&buffer[0x10]);  /* 22 */
    Bitmap_Head.biXPels = ToL(&buffer[0x14]); /* 26 */
    Bitmap_Head.biYPels = ToL(&buffer[0x18]); /* 2A */
    Bitmap_Head.biClrUsed = ToL(&buffer[0x1C]); /* 2E */
    Bitmap_Head.biClrImp = ToL(&buffer[0x20]);  /* 32 */
    Bitmap_Head.masks[0] = 0;
    Bitmap_Head.masks[1] = 0;
    Bitmap_Head.masks[2] = 0;
    Bitmap_Head.masks[3] = 0;

    Maps = 4;
    memset(masks, 0, sizeof(masks));

    if (Bitmap_Head.biCompr == BI_BITFIELDS)
      {
	if (!ReadOK(fd, buffer, 3 * sizeof(unsigned long)))
	  {
	    LOG("Error reading BMP file header\n");
	    at_exception_fatal(&exp, "Error reading BMP file header");
	    goto cleanup;
	  }

	Bitmap_Head.masks[0] = ToL(&buffer[0x00]);
	Bitmap_Head.masks[1] = ToL(&buffer[0x04]);
	Bitmap_Head.masks[2] = ToL(&buffer[0x08]);

	ReadChannelMasks(&Bitmap_Head.masks[0], masks, 3);
      }
    else if (Bitmap_Head.biCompr == BI_RGB)
      {
	setMasksDefault(Bitmap_Head.biBitCnt, masks);
      }
    else if ((Bitmap_Head.biCompr != BI_RLE4) &&
	     (Bitmap_Head.biCompr != BI_RLE8))
      {
	/* BI_ALPHABITFIELDS, etc. */
	LOG("Unsupported compression in BMP file\n");
	at_exception_fatal(&exp, "Unsupported compression in BMP file");
	goto cleanup;
      }
  }
  else if (Bitmap_File_Head.biSize >= 56 &&
	   Bitmap_File_Head.biSize <= 64)
  {
    /* enhanced Windows format with bit masks */

    if (!ReadOK (fd, buffer, Bitmap_File_Head.biSize - 4))
    {

      LOG("Error reading BMP file header\n");
      at_exception_fatal(&exp, "Error reading BMP file header");
      goto cleanup;
    }

    Bitmap_Head.biWidth = ToL(&buffer[0x00]); /* 12 */
    Bitmap_Head.biHeight = ToL(&buffer[0x04]);  /* 16 */
    Bitmap_Head.biPlanes = ToS(&buffer[0x08]);  /* 1A */
    Bitmap_Head.biBitCnt = ToS(&buffer[0x0A]);  /* 1C */
    Bitmap_Head.biCompr = ToL(&buffer[0x0C]); /* 1E */
    Bitmap_Head.biSizeIm = ToL(&buffer[0x10]);  /* 22 */
    Bitmap_Head.biXPels = ToL(&buffer[0x14]); /* 26 */
    Bitmap_Head.biYPels = ToL(&buffer[0x18]); /* 2A */
    Bitmap_Head.biClrUsed = ToL(&buffer[0x1C]); /* 2E */
    Bitmap_Head.biClrImp = ToL(&buffer[0x20]);  /* 32 */
    Bitmap_Head.masks[0] = ToL(&buffer[0x24]);       /* 36 */
    Bitmap_Head.masks[1] = ToL(&buffer[0x28]);       /* 3A */
    Bitmap_Head.masks[2] = ToL(&buffer[0x2C]);       /* 3E */
    Bitmap_Head.masks[3] = ToL(&buffer[0x30]);       /* 42 */

    Maps = 4;
    ReadChannelMasks(&Bitmap_Head.masks[0], masks, 4);
  }
  else if (Bitmap_File_Head.biSize == 108 ||
           Bitmap_File_Head.biSize == 124)
  {
    /* BMP Version 4 or 5 */

    if (!ReadOK(fd, buffer, Bitmap_File_Head.biSize - 4))
    {
	    LOG("Error reading BMP file header\n");
	    at_exception_fatal(&exp, "Error reading BMP file header");
	    goto cleanup;
    }

    Bitmap_Head.biWidth = ToL(&buffer[0x00]);
    Bitmap_Head.biHeight = ToL(&buffer[0x04]);
    Bitmap_Head.biPlanes = ToS(&buffer[0x08]);
    Bitmap_Head.biBitCnt = ToS(&buffer[0x0A]);
    Bitmap_Head.biCompr = ToL(&buffer[0x0C]);
    Bitmap_Head.biSizeIm = ToL(&buffer[0x10]);
    Bitmap_Head.biXPels = ToL(&buffer[0x14]);
    Bitmap_Head.biYPels = ToL(&buffer[0x18]);
    Bitmap_Head.biClrUsed = ToL(&buffer[0x1C]);
    Bitmap_Head.biClrImp = ToL(&buffer[0x20]);
    Bitmap_Head.masks[0] = ToL(&buffer[0x24]);
    Bitmap_Head.masks[1] = ToL(&buffer[0x28]);
    Bitmap_Head.masks[2] = ToL(&buffer[0x2C]);
    Bitmap_Head.masks[3] = ToL(&buffer[0x30]);

    Maps = 4;

    if (Bitmap_Head.biCompr == BI_BITFIELDS)
    {
	    ReadChannelMasks(&Bitmap_Head.masks[0], masks, 4);
    }
    else if (Bitmap_Head.biCompr == BI_RGB)
    {
	    setMasksDefault(Bitmap_Head.biBitCnt, masks);
    }
  } else {
    LOG("Error reading BMP file header\n");
    at_exception_fatal(&exp, "Error reading BMP file header");
    goto cleanup;
  }

  /* Valid options 1, 4, 8, 16, 24, 32 */
  /* 16 is awful, we should probably shoot whoever invented it */

  switch (Bitmap_Head.biBitCnt)
  {
  case 1:
  case 2:
  case 4:
  case 8:
  case 16:
  case 24:
  case 32:
	  break;
  default:
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  /* There should be some colors used! */

  ColormapSize = (Bitmap_File_Head.bfOffs - Bitmap_File_Head.biSize - 14) / Maps;

  if ((Bitmap_Head.biClrUsed == 0) &&
      (Bitmap_Head.biBitCnt <= 8))
  {
	  ColormapSize = Bitmap_Head.biClrUsed = 1 << Bitmap_Head.biBitCnt;
  }

  if (ColormapSize > 256)
    ColormapSize = 256;

  /* Sanity checks */

  if (Bitmap_Head.biHeight == 0 ||
	  Bitmap_Head.biWidth == 0)
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  /* biHeight may be negative, but -2147483648 is dangerous because:
	 -2147483648 == -(-2147483648) */
  if (Bitmap_Head.biWidth < 0 ||
	  Bitmap_Head.biHeight == -2147483648)
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  if (Bitmap_Head.biPlanes != 1)
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  if (Bitmap_Head.biClrUsed > 256 &&
	  Bitmap_Head.biBitCnt <= 8)
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  /* protect against integer overflows caused by malicious BMPs */
  /* use divisions in comparisons to avoid type overflows */

  if (((unsigned long)Bitmap_Head.biWidth) > (unsigned int)0x7fffffff / Bitmap_Head.biBitCnt ||
	  ((unsigned long)Bitmap_Head.biWidth) > ((unsigned int)0x7fffffff /abs(Bitmap_Head.biHeight)) / 4)
  {
	  LOG("%s is not a valid BMP file", filename);
	  at_exception_fatal(&exp, "bmp: invalid input file");
	  goto cleanup;
  }

  /* Windows and OS/2 declare filler so that rows are a multiple of
   * word length (32 bits == 4 bytes)
   */
   
  unsigned long overflowTest = Bitmap_Head.biWidth * Bitmap_Head.biBitCnt;
  if (overflowTest / Bitmap_Head.biWidth != Bitmap_Head.biBitCnt) {
    LOG("Error reading BMP file header. Width is too large\n");
    at_exception_fatal(&exp, "Error reading BMP file header. Width is too large");
    goto cleanup;
  }

  rowbytes = ((Bitmap_Head.biWidth * Bitmap_Head.biBitCnt - 1) / 32) * 4 + 4;

#ifdef DEBUG
  printf("\nSize: %u, Colors: %u, Bits: %u, Width: %u, Height: %u, Comp: %u, Zeile: %u\n", Bitmap_File_Head.bfSize, Bitmap_Head.biClrUsed, Bitmap_Head.biBitCnt, Bitmap_Head.biWidth, Bitmap_Head.biHeight, Bitmap_Head.biCompr, rowbytes);
#endif


  if (Bitmap_Head.biBitCnt <= 8)
  {
#ifdef DEBUG
    printf("Colormap read\n");
#endif
	  /* Get the Colormap */
	  if (!ReadColorMap(fd, ColorMap, ColormapSize, Maps, &Grey, &exp))
		  goto cleanup;
  }

  fseek(fd, Bitmap_File_Head.bfOffs, SEEK_SET);

  /* Get the Image and return the ID or -1 on error */
  image_storage = ReadImage(fd, 
	Bitmap_Head.biWidth, Bitmap_Head.biHeight,
	ColorMap,
        Bitmap_Head.biClrUsed,
	Bitmap_Head.biBitCnt, Bitmap_Head.biCompr, rowbytes,
        Grey,
	masks,
	&exp);

  image = at_bitmap_init(image_storage, (unsigned short)Bitmap_Head.biWidth, (unsigned short)Bitmap_Head.biHeight, Grey ? 1 : 3);
cleanup:
  fclose(fd);
  return (image);
}