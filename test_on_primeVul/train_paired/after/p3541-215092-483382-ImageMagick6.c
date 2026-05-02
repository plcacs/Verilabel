static MagickBooleanType WriteDIBImage(const ImageInfo *image_info,Image *image)
{
  DIBInfo
    dib_info;

  MagickBooleanType
    status;

  register const IndexPacket
    *indexes;

  register const PixelPacket
    *p;

  register ssize_t
    i,
    x;

  register unsigned char
    *q;

  size_t
    bytes_per_line;

  ssize_t
    y;

  unsigned char
    *dib_data,
    *pixels;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,&image->exception);
  if (status == MagickFalse)
    return(status);
  /*
    Initialize DIB raster file header.
  */
  (void) TransformImageColorspace(image,sRGBColorspace);
  if (image->storage_class == DirectClass)
    {
      /*
        Full color DIB raster.
      */
      dib_info.number_colors=0;
      dib_info.bits_per_pixel=(unsigned short) (image->matte ? 32 : 24);
    }
  else
    {
      /*
        Colormapped DIB raster.
      */
      dib_info.bits_per_pixel=8;
      if (image_info->depth > 8)
        dib_info.bits_per_pixel=16;
      if (SetImageMonochrome(image,&image->exception) != MagickFalse)
        dib_info.bits_per_pixel=1;
      dib_info.number_colors=(unsigned int) (dib_info.bits_per_pixel == 16 ? 0 :
        (1UL << dib_info.bits_per_pixel));
    }
  bytes_per_line=4*((image->columns*dib_info.bits_per_pixel+31)/32);
  dib_info.size=40;
  dib_info.width=(int) image->columns;
  dib_info.height=(int) image->rows;
  dib_info.planes=1;
  dib_info.compression=(unsigned int) (dib_info.bits_per_pixel == 16 ?
    BI_BITFIELDS : BI_RGB);
  dib_info.image_size=(unsigned int) (bytes_per_line*image->rows);
  dib_info.x_pixels=75*39;
  dib_info.y_pixels=75*39;
  switch (image->units)
  {
    case UndefinedResolution:
    case PixelsPerInchResolution:
    {
      dib_info.x_pixels=(unsigned int) (100.0*image->x_resolution/2.54);
      dib_info.y_pixels=(unsigned int) (100.0*image->y_resolution/2.54);
      break;
    }
    case PixelsPerCentimeterResolution:
    {
      dib_info.x_pixels=(unsigned int) (100.0*image->x_resolution);
      dib_info.y_pixels=(unsigned int) (100.0*image->y_resolution);
      break;
    }
  }
  dib_info.colors_important=dib_info.number_colors;
  /*
    Convert MIFF to DIB raster pixels.
  */
  pixels=(unsigned char *) AcquireQuantumMemory(image->rows,MagickMax(
    bytes_per_line,image->columns+256UL)*sizeof(*pixels));
  if (pixels == (unsigned char *) NULL)
    ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
  (void) memset(pixels,0,dib_info.image_size);
  switch (dib_info.bits_per_pixel)
  {
    case 1:
    {
      register unsigned char
        bit,
        byte;

      /*
        Convert PseudoClass image to a DIB monochrome image.
      */
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        p=GetVirtualPixels(image,0,y,image->columns,1,&image->exception);
        if (p == (const PixelPacket *) NULL)
          break;
        indexes=GetVirtualIndexQueue(image);
        q=pixels+(image->rows-y-1)*bytes_per_line;
        bit=0;
        byte=0;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          byte<<=1;
          byte|=GetPixelIndex(indexes+x) != 0 ? 0x01 : 0x00;
          bit++;
          if (bit == 8)
            {
              *q++=byte;
              bit=0;
              byte=0;
            }
           p++;
         }
         if (bit != 0)
           {
             *q++=(unsigned char) (byte << (8-bit));
             x++;
           }
        for (x=(ssize_t) (image->columns+7)/8; x < (ssize_t) bytes_per_line; x++)
          *q++=0x00;
        status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
          image->rows);
        if (status == MagickFalse)
          break;
      }
      break;
    }
    case 8:
    {
      /*
        Convert PseudoClass packet to DIB pixel.
      */
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        p=GetVirtualPixels(image,0,y,image->columns,1,&image->exception);
        if (p == (const PixelPacket *) NULL)
          break;
        indexes=GetVirtualIndexQueue(image);
        q=pixels+(image->rows-y-1)*bytes_per_line;
        for (x=0; x < (ssize_t) image->columns; x++)
          *q++=(unsigned char) GetPixelIndex(indexes+x);
        for ( ; x < (ssize_t) bytes_per_line; x++)
          *q++=0x00;
        status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
          image->rows);
        if (status == MagickFalse)
          break;
      }
      break;
    }
    case 16:
    {
      unsigned short
        word;
      /*
        Convert PseudoClass packet to DIB pixel.
      */
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        p=GetVirtualPixels(image,0,y,image->columns,1,&image->exception);
        if (p == (const PixelPacket *) NULL)
          break;
        q=pixels+(image->rows-y-1)*bytes_per_line;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          word=(unsigned short) ((ScaleColor8to5((unsigned char)
            ScaleQuantumToChar(GetPixelRed(p))) << 11) |
            (ScaleColor8to6((unsigned char) ScaleQuantumToChar(
            GetPixelGreen(p))) << 5) | (ScaleColor8to5((unsigned char)
            ScaleQuantumToChar((unsigned char) GetPixelBlue(p)) <<
            0)));
          *q++=(unsigned char)(word & 0xff);
          *q++=(unsigned char)(word >> 8);
          p++;
        }
        for (x=(ssize_t) (2*image->columns); x < (ssize_t) bytes_per_line; x++)
          *q++=0x00;
        status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
          image->rows);
        if (status == MagickFalse)
          break;
      }
      break;
    }
    case 24:
    case 32:
    {
      /*
        Convert DirectClass packet to DIB RGB pixel.
      */
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        p=GetVirtualPixels(image,0,y,image->columns,1,&image->exception);
        if (p == (const PixelPacket *) NULL)
          break;
        q=pixels+(image->rows-y-1)*bytes_per_line;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          *q++=ScaleQuantumToChar(GetPixelBlue(p));
          *q++=ScaleQuantumToChar(GetPixelGreen(p));
          *q++=ScaleQuantumToChar(GetPixelRed(p));
          if (image->matte != MagickFalse)
            *q++=ScaleQuantumToChar(GetPixelOpacity(p));
          p++;
        }
        if (dib_info.bits_per_pixel == 24)
          for (x=(ssize_t) (3*image->columns); x < (ssize_t) bytes_per_line; x++)
            *q++=0x00;
        status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
          image->rows);
        if (status == MagickFalse)
          break;
      }
      break;
    }
  }
  if (dib_info.bits_per_pixel == 8)
    if (image_info->compression != NoCompression)
      {
        size_t
          length;

        /*
          Convert run-length encoded raster pixels.
        */
        length=2UL*(bytes_per_line+2UL)+2UL;
        dib_data=(unsigned char *) AcquireQuantumMemory(length,
          (image->rows+2UL)*sizeof(*dib_data));
        if (dib_data == (unsigned char *) NULL)
          {
            pixels=(unsigned char *) RelinquishMagickMemory(pixels);
            ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
          }
        dib_info.image_size=(unsigned int) EncodeImage(image,bytes_per_line,
          pixels,dib_data);
        pixels=(unsigned char *) RelinquishMagickMemory(pixels);
        pixels=dib_data;
        dib_info.compression = BI_RLE8;
      }
  /*
    Write DIB header.
  */
  (void) WriteBlobLSBLong(image,dib_info.size);
  (void) WriteBlobLSBLong(image,(unsigned int) dib_info.width);
  (void) WriteBlobLSBLong(image,(unsigned int) dib_info.height);
  (void) WriteBlobLSBShort(image,dib_info.planes);
  (void) WriteBlobLSBShort(image,dib_info.bits_per_pixel);
  (void) WriteBlobLSBLong(image,dib_info.compression);
  (void) WriteBlobLSBLong(image,dib_info.image_size);
  (void) WriteBlobLSBLong(image,dib_info.x_pixels);
  (void) WriteBlobLSBLong(image,dib_info.y_pixels);
  (void) WriteBlobLSBLong(image,dib_info.number_colors);
  (void) WriteBlobLSBLong(image,dib_info.colors_important);
  if (image->storage_class == PseudoClass)
    {
      if (dib_info.bits_per_pixel <= 8)
        {
          unsigned char
            *dib_colormap;

          /*
            Dump colormap to file.
          */
          dib_colormap=(unsigned char *) AcquireQuantumMemory((size_t)
            (1UL << dib_info.bits_per_pixel),4*sizeof(*dib_colormap));
          if (dib_colormap == (unsigned char *) NULL)
            ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
          q=dib_colormap;
          for (i=0; i < (ssize_t) MagickMin(image->colors,dib_info.number_colors); i++)
          {
            *q++=ScaleQuantumToChar(image->colormap[i].blue);
            *q++=ScaleQuantumToChar(image->colormap[i].green);
            *q++=ScaleQuantumToChar(image->colormap[i].red);
            *q++=(Quantum) 0x0;
          }
          for ( ; i < (ssize_t) (1L << dib_info.bits_per_pixel); i++)
          {
            *q++=(Quantum) 0x0;
            *q++=(Quantum) 0x0;
            *q++=(Quantum) 0x0;
            *q++=(Quantum) 0x0;
          }
          (void) WriteBlob(image,(size_t) (4*(1 << dib_info.bits_per_pixel)),
            dib_colormap);
          dib_colormap=(unsigned char *) RelinquishMagickMemory(dib_colormap);
        }
      else
        if ((dib_info.bits_per_pixel == 16) &&
            (dib_info.compression == BI_BITFIELDS))
          {
            (void) WriteBlobLSBLong(image,0xf800);
            (void) WriteBlobLSBLong(image,0x07e0);
            (void) WriteBlobLSBLong(image,0x001f);
          }
    }
  (void) WriteBlob(image,dib_info.image_size,pixels);
  pixels=(unsigned char *) RelinquishMagickMemory(pixels);
  (void) CloseBlob(image);
  return(MagickTrue);
}