static MagickBooleanType WriteSGIImage(const ImageInfo *image_info,Image *image,
  ExceptionInfo *exception)
{
  CompressionType
    compression;

  const char
    *value;

  MagickBooleanType
    status;

  MagickOffsetType
    scene;

  MagickSizeType
    number_pixels;

  MemoryInfo
    *pixel_info;

  SGIInfo
    iris_info;

  register const Quantum
    *p;

  register ssize_t
    i,
    x;

  register unsigned char
    *q;

  size_t
    imageListLength;

  ssize_t
    y,
    z;

  unsigned char
    *pixels,
    *packets;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if ((image->columns > 65535UL) || (image->rows > 65535UL))
    ThrowWriterException(ImageError,"WidthOrHeightExceedsLimit");
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(status);
  scene=0;
  imageListLength=GetImageListLength(image);
  do
  {
    /*
      Initialize SGI raster file header.
    */
    (void) TransformImageColorspace(image,sRGBColorspace,exception);
    (void) memset(&iris_info,0,sizeof(iris_info));
    iris_info.magic=0x01DA;
    compression=image->compression;
    if (image_info->compression != UndefinedCompression)
      compression=image_info->compression;
    if (image->depth > 8)
      compression=NoCompression;
    if (compression == NoCompression)
      iris_info.storage=(unsigned char) 0x00;
    else
      iris_info.storage=(unsigned char) 0x01;
    iris_info.bytes_per_pixel=(unsigned char) (image->depth > 8 ? 2 : 1);
    iris_info.dimension=3;
    iris_info.columns=(unsigned short) image->columns;
    iris_info.rows=(unsigned short) image->rows;
    if (image->alpha_trait != UndefinedPixelTrait)
      iris_info.depth=4;
    else
      {
        if ((image_info->type != TrueColorType) &&
            (SetImageGray(image,exception) != MagickFalse))
          {
            iris_info.dimension=2;
            iris_info.depth=1;
          }
        else
          iris_info.depth=3;
      }
    iris_info.minimum_value=0;
    iris_info.maximum_value=(size_t) (image->depth <= 8 ?
      1UL*ScaleQuantumToChar(QuantumRange) :
      1UL*ScaleQuantumToShort(QuantumRange));
    /*
      Write SGI header.
    */
    (void) WriteBlobMSBShort(image,iris_info.magic);
    (void) WriteBlobByte(image,iris_info.storage);
    (void) WriteBlobByte(image,iris_info.bytes_per_pixel);
    (void) WriteBlobMSBShort(image,iris_info.dimension);
    (void) WriteBlobMSBShort(image,iris_info.columns);
    (void) WriteBlobMSBShort(image,iris_info.rows);
    (void) WriteBlobMSBShort(image,iris_info.depth);
    (void) WriteBlobMSBLong(image,(unsigned int) iris_info.minimum_value);
    (void) WriteBlobMSBLong(image,(unsigned int) iris_info.maximum_value);
    (void) WriteBlobMSBLong(image,(unsigned int) iris_info.sans);
    value=GetImageProperty(image,"label",exception);
    if (value != (const char *) NULL)
      (void) CopyMagickString(iris_info.name,value,sizeof(iris_info.name));
    (void) WriteBlob(image,sizeof(iris_info.name),(unsigned char *)
      iris_info.name);
    (void) WriteBlobMSBLong(image,(unsigned int) iris_info.pixel_format);
    (void) WriteBlob(image,sizeof(iris_info.filler),iris_info.filler);
    /*
      Allocate SGI pixels.
    */
    number_pixels=(MagickSizeType) image->columns*image->rows;
    if ((4*iris_info.bytes_per_pixel*number_pixels) !=
        ((MagickSizeType) (size_t) (4*iris_info.bytes_per_pixel*number_pixels)))
      ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
    pixel_info=AcquireVirtualMemory((size_t) number_pixels,4*
      iris_info.bytes_per_pixel*sizeof(*pixels));
    if (pixel_info == (MemoryInfo *) NULL)
      ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
    pixels=(unsigned char *) GetVirtualMemoryBlob(pixel_info);
    /*
      Convert image pixels to uncompressed SGI pixels.
    */
    for (y=0; y < (ssize_t) image->rows; y++)
    {
      p=GetVirtualPixels(image,0,y,image->columns,1,exception);
      if (p == (const Quantum *) NULL)
        break;
      if (image->depth <= 8)
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          register unsigned char
            *q;

          q=(unsigned char *) pixels;
          q+=((iris_info.rows-1)-y)*(4*iris_info.columns)+4*x;
          *q++=ScaleQuantumToChar(GetPixelRed(image,p));
          *q++=ScaleQuantumToChar(GetPixelGreen(image,p));
          *q++=ScaleQuantumToChar(GetPixelBlue(image,p));
          *q++=ScaleQuantumToChar(GetPixelAlpha(image,p));
          p+=GetPixelChannels(image);
        }
      else
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          register unsigned short
            *q;

          q=(unsigned short *) pixels;
          q+=((iris_info.rows-1)-y)*(4*iris_info.columns)+4*x;
          *q++=ScaleQuantumToShort(GetPixelRed(image,p));
          *q++=ScaleQuantumToShort(GetPixelGreen(image,p));
          *q++=ScaleQuantumToShort(GetPixelBlue(image,p));
          *q++=ScaleQuantumToShort(GetPixelAlpha(image,p));
          p+=GetPixelChannels(image);
        }
      if (image->previous == (Image *) NULL)
        {
          status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
            image->rows);
          if (status == MagickFalse)
            break;
        }
    }
    switch (compression)
    {
      case NoCompression:
      {
        /*
          Write uncompressed SGI pixels.
        */
        for (z=0; z < (ssize_t) iris_info.depth; z++)
        {
          for (y=0; y < (ssize_t) iris_info.rows; y++)
          {
            if (image->depth <= 8)
              for (x=0; x < (ssize_t) iris_info.columns; x++)
              {
                register unsigned char
                  *q;

                q=(unsigned char *) pixels;
                q+=y*(4*iris_info.columns)+4*x+z;
                (void) WriteBlobByte(image,*q);
              }
            else
              for (x=0; x < (ssize_t) iris_info.columns; x++)
              {
                register unsigned short
                  *q;

                q=(unsigned short *) pixels;
                q+=y*(4*iris_info.columns)+4*x+z;
                (void) WriteBlobMSBShort(image,*q);
              }
          }
        }
        break;
      }
      default:
      {
        MemoryInfo
          *packet_info;

        size_t
          length,
          number_packets,
          *runlength;

        ssize_t
          offset,
          *offsets;

        /*
          Convert SGI uncompressed pixels.
        */
        offsets=(ssize_t *) AcquireQuantumMemory(iris_info.rows,
          iris_info.depth*sizeof(*offsets));
        runlength=(size_t *) AcquireQuantumMemory(iris_info.rows,
          iris_info.depth*sizeof(*runlength));
        packet_info=AcquireVirtualMemory((2*(size_t) iris_info.columns+10)*
          image->rows,4*sizeof(*packets));
        if ((offsets == (ssize_t *) NULL) ||
            (runlength == (size_t *) NULL) ||
            (packet_info == (MemoryInfo *) NULL))
          {
            if (offsets != (ssize_t *) NULL)
              offsets=(ssize_t *) RelinquishMagickMemory(offsets);
            if (runlength != (size_t *) NULL)
              runlength=(size_t *) RelinquishMagickMemory(runlength);
            if (packet_info != (MemoryInfo *) NULL)
              packet_info=RelinquishVirtualMemory(packet_info);
            pixel_info=RelinquishVirtualMemory(pixel_info);
            ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
          }
        packets=(unsigned char *) GetVirtualMemoryBlob(packet_info);
        offset=512+4*2*((ssize_t) iris_info.rows*iris_info.depth);
        number_packets=0;
        q=pixels;
        for (y=0; y < (ssize_t) iris_info.rows; y++)
        {
          for (z=0; z < (ssize_t) iris_info.depth; z++)
          {
            length=SGIEncode(q+z,(size_t) iris_info.columns,packets+
              number_packets);
            number_packets+=length;
            offsets[y+z*iris_info.rows]=offset;
            runlength[y+z*iris_info.rows]=(size_t) length;
            offset+=(ssize_t) length;
          }
          q+=(iris_info.columns*4);
        }
        /*
          Write out line start and length tables and runlength-encoded pixels.
        */
        for (i=0; i < (ssize_t) (iris_info.rows*iris_info.depth); i++)
          (void) WriteBlobMSBLong(image,(unsigned int) offsets[i]);
        for (i=0; i < (ssize_t) (iris_info.rows*iris_info.depth); i++)
          (void) WriteBlobMSBLong(image,(unsigned int) runlength[i]);
        (void) WriteBlob(image,number_packets,packets);
        /*
          Relinquish resources.
        */
        offsets=(ssize_t *) RelinquishMagickMemory(offsets);
        runlength=(size_t *) RelinquishMagickMemory(runlength);
        packet_info=RelinquishVirtualMemory(packet_info);
        break;
      }
    }
    pixel_info=RelinquishVirtualMemory(pixel_info);
    if (GetNextImageInList(image) == (Image *) NULL)
      break;
    image=SyncNextImageInList(image);
    status=SetImageProgress(image,SaveImagesTag,scene++,imageListLength);
    if (status == MagickFalse)
      break;
  } while (image_info->adjoin != MagickFalse);
  (void) CloseBlob(image);
  return(MagickTrue);
}