static Image *ReadDDSImage(const ImageInfo *image_info,ExceptionInfo *exception)
{
  Image
    *image;

  MagickBooleanType
    status,
    cubemap = MagickFalse,
    volume = MagickFalse;

  CompressionType
    compression;

  DDSInfo
    dds_info;
  
  DDSDecoder
    *decoder;
  
  PixelTrait
    alpha_trait;
  
  size_t
    n,
    num_images;

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
  
  /*
    Initialize image structure.
  */
  if (ReadDDSInfo(image, &dds_info) != MagickTrue) {
    ThrowReaderException(CorruptImageError,"ImproperImageHeader");
  }
  
  if (dds_info.ddscaps2 & DDSCAPS2_CUBEMAP)
    cubemap = MagickTrue;
  
  if (dds_info.ddscaps2 & DDSCAPS2_VOLUME && dds_info.depth > 0)
    volume = MagickTrue;
  
  (void) SeekBlob(image, 128, SEEK_SET);

  /*
    Determine pixel format
  */
  if (dds_info.pixelformat.flags & DDPF_RGB)
    {
      compression = NoCompression;
      if (dds_info.pixelformat.flags & DDPF_ALPHAPIXELS)
        {
          alpha_trait = BlendPixelTrait;
          decoder = ReadUncompressedRGBA;
        }
      else
        {
          alpha_trait = UndefinedPixelTrait;
          decoder = ReadUncompressedRGB;
        }
    }
  else if (dds_info.pixelformat.flags & DDPF_LUMINANCE)
   {
      compression = NoCompression;
      if (dds_info.pixelformat.flags & DDPF_ALPHAPIXELS)
        {
          /* Not sure how to handle this */
          ThrowReaderException(CorruptImageError, "ImageTypeNotSupported");
        }
      else
        {
          alpha_trait = UndefinedPixelTrait;
          decoder = ReadUncompressedRGB;
        }
    }
  else if (dds_info.pixelformat.flags & DDPF_FOURCC)
    {
      switch (dds_info.pixelformat.fourcc)
      {
        case FOURCC_DXT1:
        {
          alpha_trait = UndefinedPixelTrait;
          compression = DXT1Compression;
          decoder = ReadDXT1;
          break;
        }
        case FOURCC_DXT3:
        {
          alpha_trait = BlendPixelTrait;
          compression = DXT3Compression;
          decoder = ReadDXT3;
          break;
        }
        case FOURCC_DXT5:
        {
          alpha_trait = BlendPixelTrait;
          compression = DXT5Compression;
          decoder = ReadDXT5;
          break;
        }
        default:
        {
          /* Unknown FOURCC */
          ThrowReaderException(CorruptImageError, "ImageTypeNotSupported");
        }
      }
    }
  else
    {
      /* Neither compressed nor uncompressed... thus unsupported */
      ThrowReaderException(CorruptImageError, "ImageTypeNotSupported");
    }
  
  num_images = 1;
  if (cubemap)
    {
      /*
        Determine number of faces defined in the cubemap
      */
      num_images = 0;
      if (dds_info.ddscaps2 & DDSCAPS2_CUBEMAP_POSITIVEX) num_images++;
      if (dds_info.ddscaps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) num_images++;
      if (dds_info.ddscaps2 & DDSCAPS2_CUBEMAP_POSITIVEY) num_images++;
      if (dds_info.ddscaps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) num_images++;
      if (dds_info.ddscaps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) num_images++;
      if (dds_info.ddscaps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) num_images++;
    }
  
  if (volume)
    num_images = dds_info.depth;
  
  for (n = 0; n < num_images; n++)
  {
    if (n != 0)
      {
        /* Start a new image */
        AcquireNextImage(image_info,image,exception);
        if (GetNextImageInList(image) == (Image *) NULL)
          return(DestroyImageList(image));
        image=SyncNextImageInList(image);
      }
    
    image->alpha_trait=alpha_trait;
    image->compression = compression;
    image->columns = dds_info.width;
    image->rows = dds_info.height;
    image->storage_class = DirectClass;
    image->endian = LSBEndian;
    image->depth = 8;
    if (image_info->ping != MagickFalse)
      {
        (void) CloseBlob(image);
        return(GetFirstImageInList(image));
      }
    status=SetImageExtent(image,image->columns,image->rows,exception);
    if (status == MagickFalse)
      return(DestroyImageList(image));
    if ((decoder)(image, &dds_info, exception) != MagickTrue)
      {
        (void) CloseBlob(image);
        return(GetFirstImageInList(image));
      }
  }
  (void) CloseBlob(image);
  return(GetFirstImageInList(image));
}