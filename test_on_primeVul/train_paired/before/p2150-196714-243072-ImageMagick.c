static Image *ReadJPEGImage(const ImageInfo *image_info,
  ExceptionInfo *exception)
{
  char
    value[MagickPathExtent];

  const char
    *option;

  ErrorManager
    error_manager;

  Image
    *image;

  JSAMPLE
    *volatile jpeg_pixels;

  JSAMPROW
    scanline[1];

  MagickBooleanType
    debug,
    status;

  MagickSizeType
    number_pixels;

  MemoryInfo
    *memory_info;

  Quantum
    index;

  register ssize_t
    i;

  struct jpeg_decompress_struct
    jpeg_info;

  struct jpeg_error_mgr
    jpeg_error;

  register JSAMPLE
    *p;

  size_t
    units;

  ssize_t
    y;

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  if (image_info->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",
      image_info->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  debug=IsEventLogging();
  (void) debug;
  image=AcquireImage(image_info,exception);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  /*
    Verify that file size large enough to contain a JPEG datastream.
  */
  if (GetBlobSize(image) < 107)
    ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
  /*
    Initialize JPEG parameters.
  */
  (void) ResetMagickMemory(&error_manager,0,sizeof(error_manager));
  (void) ResetMagickMemory(&jpeg_info,0,sizeof(jpeg_info));
  (void) ResetMagickMemory(&jpeg_error,0,sizeof(jpeg_error));
  jpeg_info.err=jpeg_std_error(&jpeg_error);
  jpeg_info.err->emit_message=(void (*)(j_common_ptr,int)) JPEGWarningHandler;
  jpeg_info.err->error_exit=(void (*)(j_common_ptr)) JPEGErrorHandler;
  memory_info=(MemoryInfo *) NULL;
  error_manager.exception=exception;
  error_manager.image=image;
  if (setjmp(error_manager.error_recovery) != 0)
    {
      jpeg_destroy_decompress(&jpeg_info);
      if (error_manager.profile != (StringInfo *) NULL)
        error_manager.profile=DestroyStringInfo(error_manager.profile);
      (void) CloseBlob(image);
      number_pixels=(MagickSizeType) image->columns*image->rows;
      if (number_pixels != 0)
        return(GetFirstImageInList(image));
      return(DestroyImage(image));
    }
  jpeg_info.client_data=(void *) &error_manager;
  jpeg_create_decompress(&jpeg_info);
  JPEGSourceManager(&jpeg_info,image);
  jpeg_set_marker_processor(&jpeg_info,JPEG_COM,ReadComment);
  option=GetImageOption(image_info,"profile:skip");
  if (IsOptionMember("ICC",option) == MagickFalse)
    jpeg_set_marker_processor(&jpeg_info,ICC_MARKER,ReadICCProfile);
  if (IsOptionMember("IPTC",option) == MagickFalse)
    jpeg_set_marker_processor(&jpeg_info,IPTC_MARKER,ReadIPTCProfile);
  for (i=1; i < 16; i++)
    if ((i != 2) && (i != 13) && (i != 14))
      if (IsOptionMember("APP",option) == MagickFalse)
        jpeg_set_marker_processor(&jpeg_info,(int) (JPEG_APP0+i),ReadProfile);
  i=(ssize_t) jpeg_read_header(&jpeg_info,TRUE);
  if ((image_info->colorspace == YCbCrColorspace) ||
      (image_info->colorspace == Rec601YCbCrColorspace) ||
      (image_info->colorspace == Rec709YCbCrColorspace))
    jpeg_info.out_color_space=JCS_YCbCr;
  /*
    Set image resolution.
  */
  units=0;
  if ((jpeg_info.saw_JFIF_marker != 0) && (jpeg_info.X_density != 1) &&
      (jpeg_info.Y_density != 1))
    {
      image->resolution.x=(double) jpeg_info.X_density;
      image->resolution.y=(double) jpeg_info.Y_density;
      units=(size_t) jpeg_info.density_unit;
    }
  if (units == 1)
    image->units=PixelsPerInchResolution;
  if (units == 2)
    image->units=PixelsPerCentimeterResolution;
  number_pixels=(MagickSizeType) image->columns*image->rows;
  option=GetImageOption(image_info,"jpeg:size");
  if ((option != (const char *) NULL) &&
      (jpeg_info.out_color_space != JCS_YCbCr))
    {
      double
        scale_factor;

      GeometryInfo
        geometry_info;

      MagickStatusType
        flags;

      /*
        Scale the image.
      */
      flags=ParseGeometry(option,&geometry_info);
      if ((flags & SigmaValue) == 0)
        geometry_info.sigma=geometry_info.rho;
      jpeg_calc_output_dimensions(&jpeg_info);
      image->magick_columns=jpeg_info.output_width;
      image->magick_rows=jpeg_info.output_height;
      scale_factor=1.0;
      if (geometry_info.rho != 0.0)
        scale_factor=jpeg_info.output_width/geometry_info.rho;
      if ((geometry_info.sigma != 0.0) &&
          (scale_factor > (jpeg_info.output_height/geometry_info.sigma)))
        scale_factor=jpeg_info.output_height/geometry_info.sigma;
      jpeg_info.scale_num=1U;
      jpeg_info.scale_denom=(unsigned int) scale_factor;
      jpeg_calc_output_dimensions(&jpeg_info);
      if (image->debug != MagickFalse)
        (void) LogMagickEvent(CoderEvent,GetMagickModule(),
          "Scale factor: %.20g",(double) scale_factor);
    }
#if (JPEG_LIB_VERSION >= 61) && defined(D_PROGRESSIVE_SUPPORTED)
#if defined(D_LOSSLESS_SUPPORTED)
  image->interlace=jpeg_info.process == JPROC_PROGRESSIVE ?
    JPEGInterlace : NoInterlace;
  image->compression=jpeg_info.process == JPROC_LOSSLESS ?
    LosslessJPEGCompression : JPEGCompression;
  if (jpeg_info.data_precision > 8)
    (void) ThrowMagickException(exception,GetMagickModule(),OptionError,
      "12-bit JPEG not supported. Reducing pixel data to 8 bits","`%s'",
      image->filename);
  if (jpeg_info.data_precision == 16)
    jpeg_info.data_precision=12;
#else
  image->interlace=jpeg_info.progressive_mode != 0 ? JPEGInterlace :
    NoInterlace;
  image->compression=JPEGCompression;
#endif
#else
  image->compression=JPEGCompression;
  image->interlace=JPEGInterlace;
#endif
  option=GetImageOption(image_info,"jpeg:colors");
  if (option != (const char *) NULL)
    {
      /*
        Let the JPEG library quantize the image.
      */
      jpeg_info.quantize_colors=TRUE;
      jpeg_info.desired_number_of_colors=(int) StringToUnsignedLong(option);
    }
  option=GetImageOption(image_info,"jpeg:block-smoothing");
  if (option != (const char *) NULL)
    jpeg_info.do_block_smoothing=IsStringTrue(option) != MagickFalse ? TRUE :
      FALSE;
  jpeg_info.dct_method=JDCT_FLOAT;
  option=GetImageOption(image_info,"jpeg:dct-method");
  if (option != (const char *) NULL)
    switch (*option)
    {
      case 'D':
      case 'd':
      {
        if (LocaleCompare(option,"default") == 0)
          jpeg_info.dct_method=JDCT_DEFAULT;
        break;
      }
      case 'F':
      case 'f':
      {
        if (LocaleCompare(option,"fastest") == 0)
          jpeg_info.dct_method=JDCT_FASTEST;
        if (LocaleCompare(option,"float") == 0)
          jpeg_info.dct_method=JDCT_FLOAT;
        break;
      }
      case 'I':
      case 'i':
      {
        if (LocaleCompare(option,"ifast") == 0)
          jpeg_info.dct_method=JDCT_IFAST;
        if (LocaleCompare(option,"islow") == 0)
          jpeg_info.dct_method=JDCT_ISLOW;
        break;
      }
    }
  option=GetImageOption(image_info,"jpeg:fancy-upsampling");
  if (option != (const char *) NULL)
    jpeg_info.do_fancy_upsampling=IsStringTrue(option) != MagickFalse ? TRUE :
      FALSE;
  (void) jpeg_start_decompress(&jpeg_info);
  image->columns=jpeg_info.output_width;
  image->rows=jpeg_info.output_height;
  image->depth=(size_t) jpeg_info.data_precision;
  switch (jpeg_info.out_color_space)
  {
    case JCS_RGB:
    default:
    {
      (void) SetImageColorspace(image,sRGBColorspace,exception);
      break;
    }
    case JCS_GRAYSCALE:
    {
      (void) SetImageColorspace(image,GRAYColorspace,exception);
      break;
    }
    case JCS_YCbCr:
    {
      (void) SetImageColorspace(image,YCbCrColorspace,exception);
      break;
    }
    case JCS_CMYK:
    {
      (void) SetImageColorspace(image,CMYKColorspace,exception);
      break;
    }
  }
  if (IsITUFaxImage(image) != MagickFalse)
    {
      (void) SetImageColorspace(image,LabColorspace,exception);
      jpeg_info.out_color_space=JCS_YCbCr;
    }
  option=GetImageOption(image_info,"jpeg:colors");
  if (option != (const char *) NULL)
    if (AcquireImageColormap(image,StringToUnsignedLong(option),exception)
         == MagickFalse)
      ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  if ((jpeg_info.output_components == 1) && (jpeg_info.quantize_colors == 0))
    {
      size_t
        colors;

      colors=(size_t) GetQuantumRange(image->depth)+1;
      if (AcquireImageColormap(image,colors,exception) == MagickFalse)
        ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
    }
  if (image->debug != MagickFalse)
    {
      if (image->interlace != NoInterlace)
        (void) LogMagickEvent(CoderEvent,GetMagickModule(),
          "Interlace: progressive");
      else
        (void) LogMagickEvent(CoderEvent,GetMagickModule(),
          "Interlace: nonprogressive");
      (void) LogMagickEvent(CoderEvent,GetMagickModule(),"Data precision: %d",
        (int) jpeg_info.data_precision);
      (void) LogMagickEvent(CoderEvent,GetMagickModule(),"Geometry: %dx%d",
        (int) jpeg_info.output_width,(int) jpeg_info.output_height);
    }
  JPEGSetImageQuality(&jpeg_info,image);
  JPEGSetImageSamplingFactor(&jpeg_info,image,exception);
  (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
    jpeg_info.out_color_space);
  (void) SetImageProperty(image,"jpeg:colorspace",value,exception);
  if (image_info->ping != MagickFalse)
    {
      jpeg_destroy_decompress(&jpeg_info);
      (void) CloseBlob(image);
      return(GetFirstImageInList(image));
    }
  status=SetImageExtent(image,image->columns,image->rows,exception);
  if (status == MagickFalse)
    {
      jpeg_destroy_decompress(&jpeg_info);
      return(DestroyImageList(image));
    }
  if ((jpeg_info.output_components != 1) &&
      (jpeg_info.output_components != 3) && (jpeg_info.output_components != 4))
    {
      jpeg_destroy_decompress(&jpeg_info);
      ThrowReaderException(CorruptImageError,"ImageTypeNotSupported");
    }
  memory_info=AcquireVirtualMemory((size_t) image->columns,
    jpeg_info.output_components*sizeof(*jpeg_pixels));
  if (memory_info == (MemoryInfo *) NULL)
    {
      jpeg_destroy_decompress(&jpeg_info);
      ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
    }
  jpeg_pixels=(JSAMPLE *) GetVirtualMemoryBlob(memory_info);
  /*
    Convert JPEG pixels to pixel packets.
  */
  if (setjmp(error_manager.error_recovery) != 0)
    {
      if (memory_info != (MemoryInfo *) NULL)
        memory_info=RelinquishVirtualMemory(memory_info);
      jpeg_destroy_decompress(&jpeg_info);
      (void) CloseBlob(image);
      number_pixels=(MagickSizeType) image->columns*image->rows;
      if (number_pixels != 0)
        return(GetFirstImageInList(image));
      return(DestroyImage(image));
    }
  if (jpeg_info.quantize_colors != 0)
    {
      image->colors=(size_t) jpeg_info.actual_number_of_colors;
      if (jpeg_info.out_color_space == JCS_GRAYSCALE)
        for (i=0; i < (ssize_t) image->colors; i++)
        {
          image->colormap[i].red=(double) ScaleCharToQuantum(
            jpeg_info.colormap[0][i]);
          image->colormap[i].green=image->colormap[i].red;
          image->colormap[i].blue=image->colormap[i].red;
          image->colormap[i].alpha=(MagickRealType) OpaqueAlpha;
        }
      else
        for (i=0; i < (ssize_t) image->colors; i++)
        {
          image->colormap[i].red=(double) ScaleCharToQuantum(
            jpeg_info.colormap[0][i]);
          image->colormap[i].green=(double) ScaleCharToQuantum(
            jpeg_info.colormap[1][i]);
          image->colormap[i].blue=(double) ScaleCharToQuantum(
            jpeg_info.colormap[2][i]);
          image->colormap[i].alpha=(MagickRealType) OpaqueAlpha;
        }
    }
  scanline[0]=(JSAMPROW) jpeg_pixels;
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    register ssize_t
      x;

    register Quantum
      *magick_restrict q;

    if (jpeg_read_scanlines(&jpeg_info,scanline,1) != 1)
      {
        (void) ThrowMagickException(exception,GetMagickModule(),
          CorruptImageWarning,"SkipToSyncByte","`%s'",image->filename);
        continue;
      }
    p=jpeg_pixels;
    q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
    if (q == (Quantum *) NULL)
      break;
    if (jpeg_info.data_precision > 8)
      {
        unsigned short
          scale;

        scale=65535/(unsigned short) GetQuantumRange((size_t)
          jpeg_info.data_precision);
        if (jpeg_info.output_components == 1)
          for (x=0; x < (ssize_t) image->columns; x++)
          {
            ssize_t
              pixel;

            pixel=(ssize_t) (scale*GETJSAMPLE(*p));
            index=(Quantum) ConstrainColormapIndex(image,pixel,exception);
            SetPixelIndex(image,index,q);
            SetPixelViaPixelInfo(image,image->colormap+(ssize_t) index,q);
            p++;
            q+=GetPixelChannels(image);
          }
        else
          if (image->colorspace != CMYKColorspace)
            for (x=0; x < (ssize_t) image->columns; x++)
            {
              SetPixelRed(image,ScaleShortToQuantum(
                (unsigned short) (scale*GETJSAMPLE(*p++))),q);
              SetPixelGreen(image,ScaleShortToQuantum(
                (unsigned short) (scale*GETJSAMPLE(*p++))),q);
              SetPixelBlue(image,ScaleShortToQuantum(
                (unsigned short) (scale*GETJSAMPLE(*p++))),q);
              SetPixelAlpha(image,OpaqueAlpha,q);
              q+=GetPixelChannels(image);
            }
          else
            for (x=0; x < (ssize_t) image->columns; x++)
            {
              SetPixelCyan(image,QuantumRange-ScaleShortToQuantum(
                (unsigned short) (scale*GETJSAMPLE(*p++))),q);
              SetPixelMagenta(image,QuantumRange-ScaleShortToQuantum(
                (unsigned short) (scale*GETJSAMPLE(*p++))),q);
              SetPixelYellow(image,QuantumRange-ScaleShortToQuantum(
                (unsigned short) (scale*GETJSAMPLE(*p++))),q);
              SetPixelBlack(image,QuantumRange-ScaleShortToQuantum(
                (unsigned short) (scale*GETJSAMPLE(*p++))),q);
              SetPixelAlpha(image,OpaqueAlpha,q);
              q+=GetPixelChannels(image);
            }
      }
    else
      if (jpeg_info.output_components == 1)
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          index=(Quantum) ConstrainColormapIndex(image,(ssize_t) GETJSAMPLE(*p),
            exception);
          SetPixelIndex(image,index,q);
          SetPixelViaPixelInfo(image,image->colormap+(ssize_t) index,q);
          p++;
          q+=GetPixelChannels(image);
        }
      else
        if (image->colorspace != CMYKColorspace)
          for (x=0; x < (ssize_t) image->columns; x++)
          {
            SetPixelRed(image,ScaleCharToQuantum((unsigned char)
              GETJSAMPLE(*p++)),q);
            SetPixelGreen(image,ScaleCharToQuantum((unsigned char)
              GETJSAMPLE(*p++)),q);
            SetPixelBlue(image,ScaleCharToQuantum((unsigned char)
              GETJSAMPLE(*p++)),q);
            SetPixelAlpha(image,OpaqueAlpha,q);
            q+=GetPixelChannels(image);
          }
        else
          for (x=0; x < (ssize_t) image->columns; x++)
          {
            SetPixelCyan(image,QuantumRange-ScaleCharToQuantum(
              (unsigned char) GETJSAMPLE(*p++)),q);
            SetPixelMagenta(image,QuantumRange-ScaleCharToQuantum(
              (unsigned char) GETJSAMPLE(*p++)),q);
            SetPixelYellow(image,QuantumRange-ScaleCharToQuantum(
              (unsigned char) GETJSAMPLE(*p++)),q);
            SetPixelBlack(image,QuantumRange-ScaleCharToQuantum(
              (unsigned char) GETJSAMPLE(*p++)),q);
            SetPixelAlpha(image,OpaqueAlpha,q);
            q+=GetPixelChannels(image);
          }
    if (SyncAuthenticPixels(image,exception) == MagickFalse)
      break;
    status=SetImageProgress(image,LoadImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      {
        jpeg_abort_decompress(&jpeg_info);
        break;
      }
  }
  if (status != MagickFalse)
    {
      error_manager.finished=MagickTrue;
      if (setjmp(error_manager.error_recovery) == 0)
        (void) jpeg_finish_decompress(&jpeg_info);
    }
  /*
    Free jpeg resources.
  */
  jpeg_destroy_decompress(&jpeg_info);
  memory_info=RelinquishVirtualMemory(memory_info);
  (void) CloseBlob(image);
  return(GetFirstImageInList(image));
}