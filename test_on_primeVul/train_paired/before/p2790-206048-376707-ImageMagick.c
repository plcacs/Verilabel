static Image *ReadWEBPImage(const ImageInfo *image_info,
  ExceptionInfo *exception)
{
  Image
    *image;

  int
    webp_status;

  MagickBooleanType
    status;

  register unsigned char
    *p;

  size_t
    length;

  ssize_t
    count,
    y;

  unsigned char
    header[12],
    *stream;

  WebPDecoderConfig
    configure;

  WebPDecBuffer
    *magick_restrict webp_image = &configure.output;

  WebPBitstreamFeatures
    *magick_restrict features = &configure.input;

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
  image=AcquireImage(image_info,exception);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  if (WebPInitDecoderConfig(&configure) == 0)
    ThrowReaderException(ResourceLimitError,"UnableToDecodeImageFile");
  webp_image->colorspace=MODE_RGBA;
  count=ReadBlob(image,12,header);
  if (count != 12)
    ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
  status=IsWEBP(header,count);
  if (status == MagickFalse)
    ThrowReaderException(CorruptImageError,"CorruptImage");
  length=(size_t) (ReadWebPLSBWord(header+4)+8);
  if (length < 12)
    ThrowReaderException(CorruptImageError,"CorruptImage");
  stream=(unsigned char *) AcquireQuantumMemory(length,sizeof(*stream));
  if (stream == (unsigned char *) NULL)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  memcpy(stream,header,12);
  count=ReadBlob(image,length-12,stream+12);
  if (count != (ssize_t) (length-12))
    {
      stream=(unsigned char*) RelinquishMagickMemory(stream);
      ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
    }
  webp_status=WebPGetFeatures(stream,length,features);
  if (webp_status == VP8_STATUS_OK)
    {
      image->columns=(size_t) features->width;
      image->rows=(size_t) features->height;
      image->depth=8;
      image->alpha_trait=features->has_alpha != 0 ? BlendPixelTrait :
        UndefinedPixelTrait;
      if (image_info->ping != MagickFalse)
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          (void) CloseBlob(image);
          return(GetFirstImageInList(image));
        }
      status=SetImageExtent(image,image->columns,image->rows,exception);
      if (status == MagickFalse)
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          (void) CloseBlob(image);
          return(DestroyImageList(image));
        }
      webp_status=WebPDecode(stream,length,&configure);
    }
  if (webp_status != VP8_STATUS_OK)
    {
      stream=(unsigned char*) RelinquishMagickMemory(stream);
      switch (webp_status)
      {
        case VP8_STATUS_OUT_OF_MEMORY:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
          break;
        }
        case VP8_STATUS_INVALID_PARAM:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(CorruptImageError,"invalid parameter");
          break;
        }
        case VP8_STATUS_BITSTREAM_ERROR:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(CorruptImageError,"CorruptImage");
          break;
        }
        case VP8_STATUS_UNSUPPORTED_FEATURE:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(CoderError,"DataEncodingSchemeIsNotSupported");
          break;
        }
        case VP8_STATUS_SUSPENDED:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(CorruptImageError,"decoder suspended");
          break;
        }
        case VP8_STATUS_USER_ABORT:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(CorruptImageError,"user abort");
          break;
        }
        case VP8_STATUS_NOT_ENOUGH_DATA:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
          break;
        }
        default:
        {
          stream=(unsigned char*) RelinquishMagickMemory(stream);
          ThrowReaderException(CorruptImageError,"CorruptImage");
        }
      }
    }
  if (IsWEBPImageLossless(stream,length) != MagickFalse)
    image->quality=100;
  p=(unsigned char *) webp_image->u.RGBA.rgba;
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    register Quantum
      *q;

    register ssize_t
      x;

    q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
    if (q == (Quantum *) NULL)
      break;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      SetPixelRed(image,ScaleCharToQuantum(*p++),q);
      SetPixelGreen(image,ScaleCharToQuantum(*p++),q);
      SetPixelBlue(image,ScaleCharToQuantum(*p++),q);
      SetPixelAlpha(image,ScaleCharToQuantum(*p++),q);
      q+=GetPixelChannels(image);
    }
    if (SyncAuthenticPixels(image,exception) == MagickFalse)
      break;
    status=SetImageProgress(image,LoadImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      break;
  }
  WebPFreeDecBuffer(webp_image);
  stream=(unsigned char*) RelinquishMagickMemory(stream);
  (void) CloseBlob(image);
  return(image);
}