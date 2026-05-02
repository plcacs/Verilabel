static Image *ReadDIBImage(const ImageInfo *image_info,ExceptionInfo *exception)
{
  DIBInfo
    dib_info;

  Image
    *image;

  IndexPacket
    index;

  ssize_t
    bit,
    y;

  MagickBooleanType
    status;

  MemoryInfo
    *pixel_info;

  register IndexPacket
    *indexes;

  register ssize_t
    x;

  register PixelPacket
    *q;

  register ssize_t
    i;

  register unsigned char
    *p;

  size_t
    bytes_per_line,
    length;

  ssize_t
    count;

  unsigned char
    *pixels;

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
  image=AcquireImage(image_info);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  /*
    Determine if this a DIB file.
  */
  (void) memset(&dib_info,0,sizeof(dib_info));
  dib_info.size=ReadBlobLSBLong(image);
  if (dib_info.size != 40)
    ThrowReaderException(CorruptImageError,"ImproperImageHeader");
  /*
    Microsoft Windows 3.X DIB image file.
  */
  dib_info.width=ReadBlobLSBSignedLong(image);
  dib_info.height=ReadBlobLSBSignedLong(image);
  dib_info.planes=ReadBlobLSBShort(image);
  dib_info.bits_per_pixel=ReadBlobLSBShort(image);
  dib_info.compression=ReadBlobLSBLong(image);
  dib_info.image_size=ReadBlobLSBLong(image);
  dib_info.x_pixels=ReadBlobLSBLong(image);
  dib_info.y_pixels=ReadBlobLSBLong(image);
  dib_info.number_colors=ReadBlobLSBLong(image);
  dib_info.colors_important=ReadBlobLSBLong(image);
  if ((dib_info.bits_per_pixel != 1) && (dib_info.bits_per_pixel != 4) &&
      (dib_info.bits_per_pixel != 8) && (dib_info.bits_per_pixel != 16) &&
      (dib_info.bits_per_pixel != 24) && (dib_info.bits_per_pixel != 32))
    ThrowReaderException(CorruptImageError,"ImproperImageHeader");
  if ((dib_info.compression == BI_BITFIELDS) &&
      ((dib_info.bits_per_pixel == 16) || (dib_info.bits_per_pixel == 32)))
    {
      dib_info.red_mask=(unsigned short) ReadBlobLSBLong(image);
      dib_info.green_mask=(unsigned short) ReadBlobLSBLong(image);
      dib_info.blue_mask=(unsigned short) ReadBlobLSBLong(image);
    }
  if (EOFBlob(image) != MagickFalse)
    ThrowReaderException(CorruptImageError,"UnexpectedEndOfFile");
  if (dib_info.width <= 0)
    ThrowReaderException(CorruptImageError,"NegativeOrZeroImageSize");
  if (dib_info.height == 0)
    ThrowReaderException(CorruptImageError,"NegativeOrZeroImageSize");
  if (dib_info.planes != 1)
    ThrowReaderException(CorruptImageError,"StaticPlanesValueNotEqualToOne");
  if ((dib_info.bits_per_pixel != 1) && (dib_info.bits_per_pixel != 4) &&
      (dib_info.bits_per_pixel != 8) && (dib_info.bits_per_pixel != 16) &&
      (dib_info.bits_per_pixel != 24) && (dib_info.bits_per_pixel != 32))
    ThrowReaderException(CorruptImageError,"UnrecognizedBitsPerPixel");
  if ((dib_info.bits_per_pixel < 16) &&
      (dib_info.number_colors > (unsigned int) (1UL << dib_info.bits_per_pixel)))
    ThrowReaderException(CorruptImageError,"UnrecognizedNumberOfColors");
  if ((dib_info.compression == 1) && (dib_info.bits_per_pixel != 8))
    ThrowReaderException(CorruptImageError,"UnrecognizedBitsPerPixel");
  if ((dib_info.compression == 2) && (dib_info.bits_per_pixel != 4))
    ThrowReaderException(CorruptImageError,"UnrecognizedBitsPerPixel");
  if ((dib_info.compression == 3) && (dib_info.bits_per_pixel < 16))
    ThrowReaderException(CorruptImageError,"UnrecognizedBitsPerPixel");
  switch (dib_info.compression)
  {
    case BI_RGB:
    case BI_RLE8:
    case BI_RLE4:
    case BI_BITFIELDS:
      break;
    case BI_JPEG:
      ThrowReaderException(CoderError,"JPEGCompressNotSupported");
    case BI_PNG:
      ThrowReaderException(CoderError,"PNGCompressNotSupported");
    default:
      ThrowReaderException(CorruptImageError,"UnrecognizedImageCompression");
  }
  image->columns=(size_t) MagickAbsoluteValue((ssize_t) dib_info.width);
  image->rows=(size_t) MagickAbsoluteValue((ssize_t) dib_info.height);
  image->matte=dib_info.bits_per_pixel == 32 ? MagickTrue : MagickFalse;
  image->depth=8;
  if ((dib_info.number_colors > 256) || (dib_info.colors_important > 256))
    ThrowReaderException(CorruptImageError,"ImproperImageHeader");
  if ((dib_info.number_colors != 0) || (dib_info.bits_per_pixel < 16))
    {
      size_t
        one;

      image->storage_class=PseudoClass;
      image->colors=dib_info.number_colors;
      one=1;
      if (image->colors == 0)
        image->colors=one << dib_info.bits_per_pixel;
    }
  if (image_info->size != (char *) NULL)
    {
      RectangleInfo
        geometry;

      MagickStatusType
        flags;

      flags=ParseAbsoluteGeometry(image_info->size,&geometry);
      if (flags & WidthValue)
        if ((geometry.width != 0) && (geometry.width < image->columns))
          image->columns=geometry.width;
      if (flags & HeightValue)
        if ((geometry.height != 0) && (geometry.height < image->rows))
          image->rows=geometry.height;
    }
  status=SetImageExtent(image,image->columns,image->rows);
  if (status == MagickFalse)
    {
      InheritException(exception,&image->exception);
      return(DestroyImageList(image));
    }
  if (image->storage_class == PseudoClass)
    {
      size_t
        length,
        packet_size;

      unsigned char
        *dib_colormap;

      /*
        Read DIB raster colormap.
      */
      if (AcquireImageColormap(image,image->colors) == MagickFalse)
        ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
      length=(size_t) image->colors;
      dib_colormap=(unsigned char *) AcquireQuantumMemory(length,
        4*sizeof(*dib_colormap));
      if (dib_colormap == (unsigned char *) NULL)
        ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
      packet_size=4;
      count=ReadBlob(image,packet_size*image->colors,dib_colormap);
      if (count != (ssize_t) (packet_size*image->colors))
        {
          dib_colormap=(unsigned char *) RelinquishMagickMemory(dib_colormap);
          ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
        }
      p=dib_colormap;
      for (i=0; i < (ssize_t) image->colors; i++)
      {
        image->colormap[i].blue=ScaleCharToQuantum(*p++);
        image->colormap[i].green=ScaleCharToQuantum(*p++);
        image->colormap[i].red=ScaleCharToQuantum(*p++);
        if (packet_size == 4)
          p++;
      }
      dib_colormap=(unsigned char *) RelinquishMagickMemory(dib_colormap);
    }
  /*
    Read image data.
  */
  if (dib_info.compression == BI_RLE4)
    dib_info.bits_per_pixel<<=1;
  bytes_per_line=4*((image->columns*dib_info.bits_per_pixel+31)/32);
  length=bytes_per_line*image->rows;
  pixel_info=AcquireVirtualMemory(image->rows,MagickMax(bytes_per_line,
    image->columns+256UL)*sizeof(*pixels));
  if (pixel_info == (MemoryInfo *) NULL)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  pixels=(unsigned char *) GetVirtualMemoryBlob(pixel_info);
  if ((dib_info.compression == BI_RGB) ||
      (dib_info.compression == BI_BITFIELDS))
    {
      count=ReadBlob(image,length,pixels);
      if (count != (ssize_t) (length))
        {
          pixel_info=RelinquishVirtualMemory(pixel_info);
          ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
        }
    }
  else
    {
      /*
        Convert run-length encoded raster pixels.
      */
      status=DecodeImage(image,dib_info.compression ? MagickTrue : MagickFalse,
        pixels,image->columns*image->rows);
      if (status == MagickFalse)
        {
          pixel_info=RelinquishVirtualMemory(pixel_info);
          ThrowReaderException(CorruptImageError,
            "UnableToRunlengthDecodeImage");
        }
    }
  /*
    Initialize image structure.
  */
  image->units=PixelsPerCentimeterResolution;
  image->x_resolution=(double) dib_info.x_pixels/100.0;
  image->y_resolution=(double) dib_info.y_pixels/100.0;
  /*
    Convert DIB raster image to pixel packets.
  */
  switch (dib_info.bits_per_pixel)
  {
    case 1:
    {
      /*
        Convert bitmap scanline.
      */
      for (y=(ssize_t) image->rows-1; y >= 0; y--)
      {
        p=pixels+(image->rows-y-1)*bytes_per_line;
        q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
        if (q == (PixelPacket *) NULL)
          break;
        indexes=GetAuthenticIndexQueue(image);
        for (x=0; x < ((ssize_t) image->columns-7); x+=8)
        {
          for (bit=0; bit < 8; bit++)
          {
            index=(IndexPacket) ((*p) & (0x80 >> bit) ? 0x01 : 0x00);
            SetPixelIndex(indexes+x+bit,index);
          }
          p++;
        }
        if ((image->columns % 8) != 0)
          {
            for (bit=0; bit < (ssize_t) (image->columns % 8); bit++)
            {
              index=(IndexPacket) ((*p) & (0x80 >> bit) ? 0x01 : 0x00);
              SetPixelIndex(indexes+x+bit,index);
            }
            p++;
          }
        if (SyncAuthenticPixels(image,exception) == MagickFalse)
          break;
        if (image->previous == (Image *) NULL)
          {
            status=SetImageProgress(image,LoadImageTag,(MagickOffsetType)
              image->rows-y-1,image->rows);
            if (status == MagickFalse)
              break;
          }
      }
      (void) SyncImage(image);
      break;
    }
    case 4:
    {
      /*
        Convert PseudoColor scanline.
      */
      for (y=(ssize_t) image->rows-1; y >= 0; y--)
      {
        p=pixels+(image->rows-y-1)*bytes_per_line;
        q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
        if (q == (PixelPacket *) NULL)
          break;
        indexes=GetAuthenticIndexQueue(image);
        for (x=0; x < ((ssize_t) image->columns-1); x+=2)
        {
          index=ConstrainColormapIndex(image,(*p >> 4) & 0xf);
          SetPixelIndex(indexes+x,index);
          index=ConstrainColormapIndex(image,*p & 0xf);
          SetPixelIndex(indexes+x+1,index);
          p++;
        }
        if ((image->columns % 2) != 0)
          {
            index=ConstrainColormapIndex(image,(*p >> 4) & 0xf);
            SetPixelIndex(indexes+x,index);
            p++;
          }
        if (SyncAuthenticPixels(image,exception) == MagickFalse)
          break;
        if (image->previous == (Image *) NULL)
          {
            status=SetImageProgress(image,LoadImageTag,(MagickOffsetType)
              image->rows-y-1,image->rows);
            if (status == MagickFalse)
              break;
          }
      }
      (void) SyncImage(image);
      break;
    }
    case 8:
    {
      /*
        Convert PseudoColor scanline.
      */
      if ((dib_info.compression == BI_RLE8) ||
          (dib_info.compression == BI_RLE4))
        bytes_per_line=image->columns;
      for (y=(ssize_t) image->rows-1; y >= 0; y--)
      {
        p=pixels+(image->rows-y-1)*bytes_per_line;
        q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
        if (q == (PixelPacket *) NULL)
          break;
        indexes=GetAuthenticIndexQueue(image);
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          index=ConstrainColormapIndex(image,*p);
          SetPixelIndex(indexes+x,index);
          p++;
          q++;
        }
        if (SyncAuthenticPixels(image,exception) == MagickFalse)
          break;
        if (image->previous == (Image *) NULL)
          {
            status=SetImageProgress(image,LoadImageTag,(MagickOffsetType)
              image->rows-y-1,image->rows);
            if (status == MagickFalse)
              break;
          }
      }
      (void) SyncImage(image);
      break;
    }
    case 16:
    {
      unsigned short
        word;

      /*
        Convert PseudoColor scanline.
      */
      image->storage_class=DirectClass;
      if (dib_info.compression == BI_RLE8)
        bytes_per_line=2*image->columns;
      for (y=(ssize_t) image->rows-1; y >= 0; y--)
      {
        p=pixels+(image->rows-y-1)*bytes_per_line;
        q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
        if (q == (PixelPacket *) NULL)
          break;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          word=(*p++);
          word|=(*p++ << 8);
          if (dib_info.red_mask == 0)
            {
              SetPixelRed(q,ScaleCharToQuantum(ScaleColor5to8(
                (unsigned char) ((word >> 10) & 0x1f))));
              SetPixelGreen(q,ScaleCharToQuantum(ScaleColor5to8(
                (unsigned char) ((word >> 5) & 0x1f))));
              SetPixelBlue(q,ScaleCharToQuantum(ScaleColor5to8(
                (unsigned char) (word & 0x1f))));
            }
          else
            {
              SetPixelRed(q,ScaleCharToQuantum(ScaleColor5to8(
                (unsigned char) ((word >> 11) & 0x1f))));
              SetPixelGreen(q,ScaleCharToQuantum(ScaleColor6to8(
                (unsigned char) ((word >> 5) & 0x3f))));
              SetPixelBlue(q,ScaleCharToQuantum(ScaleColor5to8(
                (unsigned char) (word & 0x1f))));
            }
          q++;
        }
        if (SyncAuthenticPixels(image,exception) == MagickFalse)
          break;
        if (image->previous == (Image *) NULL)
          {
            status=SetImageProgress(image,LoadImageTag,(MagickOffsetType)
              image->rows-y-1,image->rows);
            if (status == MagickFalse)
              break;
          }
      }
      break;
    }
    case 24:
    case 32:
    {
      /*
        Convert DirectColor scanline.
      */
      for (y=(ssize_t) image->rows-1; y >= 0; y--)
      {
        p=pixels+(image->rows-y-1)*bytes_per_line;
        q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
        if (q == (PixelPacket *) NULL)
          break;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          SetPixelBlue(q,ScaleCharToQuantum(*p++));
          SetPixelGreen(q,ScaleCharToQuantum(*p++));
          SetPixelRed(q,ScaleCharToQuantum(*p++));
          if (image->matte != MagickFalse)
            SetPixelOpacity(q,ScaleCharToQuantum(*p++));
          q++;
        }
        if (SyncAuthenticPixels(image,exception) == MagickFalse)
          break;
        if (image->previous == (Image *) NULL)
          {
            status=SetImageProgress(image,LoadImageTag,(MagickOffsetType)
              image->rows-y-1,image->rows);
            if (status == MagickFalse)
              break;
          }
      }
      break;
    }
    default:
    {
      pixel_info=RelinquishVirtualMemory(pixel_info);
      ThrowReaderException(CorruptImageError,"ImproperImageHeader");
    }
  }
  pixel_info=RelinquishVirtualMemory(pixel_info);
  if (strcmp(image_info->magick,"ICODIB") == 0)
    {
      int
        c;

      /*
        Handle ICO mask.
      */
      image->storage_class=DirectClass;
      image->matte=MagickTrue;
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        register ssize_t
          x;

        register PixelPacket
          *magick_restrict q;

        q=GetAuthenticPixels(image,0,y,image->columns,1,exception);
        if (q == (PixelPacket *) NULL)
          break;
        for (x=0; x < ((ssize_t) image->columns-7); x+=8)
        {
          c=ReadBlobByte(image);
          for (bit=0; bit < 8; bit++)
            SetPixelOpacity(q+x+bit,c & (0x80 >> bit) ? TransparentOpacity :
              OpaqueOpacity);
        }
        if ((image->columns % 8) != 0)
          {
            c=ReadBlobByte(image);
            for (bit=0; bit < (ssize_t) (image->columns % 8); bit++)
              SetPixelOpacity(q+x+bit,c & (0x80 >> bit) ? TransparentOpacity :
                OpaqueOpacity);
          }
        if (image->columns % 32)
          for (x=0; x < (ssize_t) ((32-(image->columns % 32))/8); x++)
            c=ReadBlobByte(image);
        if (SyncAuthenticPixels(image,exception) == MagickFalse)
          break;
      }
    }
  if (dib_info.height < 0)
    {
      Image
        *flipped_image;

      /*
        Correct image orientation.
      */
      flipped_image=FlipImage(image,exception);
      if (flipped_image != (Image *) NULL)
        {
          DuplicateBlob(flipped_image,image);
          image=DestroyImage(image);
          image=flipped_image;
        }
    }
  if (EOFBlob(image) != MagickFalse)
    ThrowFileException(exception,CorruptImageError,"UnexpectedEndOfFile",
      image->filename);
  (void) CloseBlob(image);
  return(GetFirstImageInList(image));
}