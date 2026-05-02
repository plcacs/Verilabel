static MagickBooleanType WriteGIFImage(const ImageInfo *image_info,Image *image,
  ExceptionInfo *exception)
{
  int
    c;

  ImageInfo
    *write_info;

  MagickBooleanType
    status;

  MagickOffsetType
    scene;

  RectangleInfo
    page;

  register ssize_t
    i;

  register unsigned char
    *q;

  size_t
    bits_per_pixel,
    delay,
    length,
    one;

  ssize_t
    j,
    opacity;

  unsigned char
    *colormap,
    *global_colormap;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(status);
  /*
    Allocate colormap.
  */
  global_colormap=(unsigned char *) AcquireQuantumMemory(768UL,
    sizeof(*global_colormap));
  colormap=(unsigned char *) AcquireQuantumMemory(768UL,sizeof(*colormap));
  if ((global_colormap == (unsigned char *) NULL) ||
      (colormap == (unsigned char *) NULL))
    {
      if (global_colormap != (unsigned char *) NULL)
        global_colormap=(unsigned char *) RelinquishMagickMemory(
          global_colormap);
      if (colormap != (unsigned char *) NULL)
        colormap=(unsigned char *) RelinquishMagickMemory(colormap);
      ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
    }
  for (i=0; i < 768; i++)
    colormap[i]=(unsigned char) 0;
  /*
    Write GIF header.
  */
  write_info=CloneImageInfo(image_info);
  if (LocaleCompare(write_info->magick,"GIF87") != 0)
    (void) WriteBlob(image,6,(unsigned char *) "GIF89a");
  else
    {
      (void) WriteBlob(image,6,(unsigned char *) "GIF87a");
      write_info->adjoin=MagickFalse;
    }
  /*
    Determine image bounding box.
  */
  page.width=image->columns;
  if (image->page.width > page.width)
    page.width=image->page.width;
  page.height=image->rows;
  if (image->page.height > page.height)
    page.height=image->page.height;
  page.x=image->page.x;
  page.y=image->page.y;
  (void) WriteBlobLSBShort(image,(unsigned short) page.width);
  (void) WriteBlobLSBShort(image,(unsigned short) page.height);
  /*
    Write images to file.
  */
  if ((write_info->adjoin != MagickFalse) &&
      (GetNextImageInList(image) != (Image *) NULL))
    write_info->interlace=NoInterlace;
  scene=0;
  one=1;
  do
  {
    (void) TransformImageColorspace(image,sRGBColorspace,exception);
    opacity=(-1);
    if (IsImageOpaque(image,exception) != MagickFalse)
      {
        if ((image->storage_class == DirectClass) || (image->colors > 256))
          (void) SetImageType(image,PaletteType,exception);
      }
    else
      {
        double
          alpha,
          beta;

        /*
          Identify transparent colormap index.
        */
        if ((image->storage_class == DirectClass) || (image->colors > 256))
          (void) SetImageType(image,PaletteBilevelAlphaType,exception);
        for (i=0; i < (ssize_t) image->colors; i++)
          if (image->colormap[i].alpha != OpaqueAlpha)
            {
              if (opacity < 0)
                {
                  opacity=i;
                  continue;
                }
              alpha=fabs(image->colormap[i].alpha-TransparentAlpha);
              beta=fabs(image->colormap[opacity].alpha-TransparentAlpha);
              if (alpha < beta)
                opacity=i;
            }
        if (opacity == -1)
          {
            (void) SetImageType(image,PaletteBilevelAlphaType,exception);
            for (i=0; i < (ssize_t) image->colors; i++)
              if (image->colormap[i].alpha != OpaqueAlpha)
                {
                  if (opacity < 0)
                    {
                      opacity=i;
                      continue;
                    }
                  alpha=fabs(image->colormap[i].alpha-TransparentAlpha);
                  beta=fabs(image->colormap[opacity].alpha-TransparentAlpha);
                  if (alpha < beta)
                    opacity=i;
                }
          }
        if (opacity >= 0)
          {
            image->colormap[opacity].red=image->transparent_color.red;
            image->colormap[opacity].green=image->transparent_color.green;
            image->colormap[opacity].blue=image->transparent_color.blue;
          }
      }
    if ((image->storage_class == DirectClass) || (image->colors > 256))
      ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
    for (bits_per_pixel=1; bits_per_pixel < 8; bits_per_pixel++)
      if ((one << bits_per_pixel) >= image->colors)
        break;
    q=colormap;
    for (i=0; i < (ssize_t) image->colors; i++)
    {
      *q++=ScaleQuantumToChar(ClampToQuantum(image->colormap[i].red));
      *q++=ScaleQuantumToChar(ClampToQuantum(image->colormap[i].green));
      *q++=ScaleQuantumToChar(ClampToQuantum(image->colormap[i].blue));
    }
    for ( ; i < (ssize_t) (one << bits_per_pixel); i++)
    {
      *q++=(unsigned char) 0x0;
      *q++=(unsigned char) 0x0;
      *q++=(unsigned char) 0x0;
    }
    if ((GetPreviousImageInList(image) == (Image *) NULL) ||
        (write_info->adjoin == MagickFalse))
      {
        /*
          Write global colormap.
        */
        c=0x80;
        c|=(8-1) << 4;  /* color resolution */
        c|=(bits_per_pixel-1);   /* size of global colormap */
        (void) WriteBlobByte(image,(unsigned char) c);
        for (j=0; j < (ssize_t) image->colors; j++)
          if (IsPixelInfoEquivalent(&image->background_color,image->colormap+j))
            break;
        (void) WriteBlobByte(image,(unsigned char)
          (j == (ssize_t) image->colors ? 0 : j));  /* background color */
        (void) WriteBlobByte(image,(unsigned char) 0x00);  /* reserved */
        length=(size_t) (3*(one << bits_per_pixel));
        (void) WriteBlob(image,length,colormap);
        for (j=0; j < 768; j++)
          global_colormap[j]=colormap[j];
      }
    if (LocaleCompare(write_info->magick,"GIF87") != 0)
      {
        const char
          *value;

        /*
          Write graphics control extension.
        */
        (void) WriteBlobByte(image,(unsigned char) 0x21);
        (void) WriteBlobByte(image,(unsigned char) 0xf9);
        (void) WriteBlobByte(image,(unsigned char) 0x04);
        c=image->dispose << 2;
        if (opacity >= 0)
          c|=0x01;
        (void) WriteBlobByte(image,(unsigned char) c);
        delay=(size_t) (100*image->delay/MagickMax((size_t)
          image->ticks_per_second,1));
        (void) WriteBlobLSBShort(image,(unsigned short) delay);
        (void) WriteBlobByte(image,(unsigned char) (opacity >= 0 ? opacity :
          0));
        (void) WriteBlobByte(image,(unsigned char) 0x00);
        value=GetImageProperty(image,"comment",exception);
        if ((LocaleCompare(write_info->magick,"GIF87") != 0) &&
            (value != (const char *) NULL))
          {
            register const char 
              *p;

            size_t
              count;
    
            /*
              Write comment extension.
            */
            (void) WriteBlobByte(image,(unsigned char) 0x21);
            (void) WriteBlobByte(image,(unsigned char) 0xfe);
            for (p=value; *p != '\0'; )
            {
              count=MagickMin(strlen(p),255);
              (void) WriteBlobByte(image,(unsigned char) count);
              for (i=0; i < (ssize_t) count; i++)
                (void) WriteBlobByte(image,(unsigned char) *p++);
            }
            (void) WriteBlobByte(image,(unsigned char) 0x00);
          }
        if ((GetPreviousImageInList(image) == (Image *) NULL) &&
            (GetNextImageInList(image) != (Image *) NULL) &&
            (image->iterations != 1))
          {
            /*
              Write Netscape Loop extension.
            */
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
               "  Writing GIF Extension %s","NETSCAPE2.0");
            (void) WriteBlobByte(image,(unsigned char) 0x21);
            (void) WriteBlobByte(image,(unsigned char) 0xff);
            (void) WriteBlobByte(image,(unsigned char) 0x0b);
            (void) WriteBlob(image,11,(unsigned char *) "NETSCAPE2.0");
            (void) WriteBlobByte(image,(unsigned char) 0x03);
            (void) WriteBlobByte(image,(unsigned char) 0x01);
            (void) WriteBlobLSBShort(image,(unsigned short) (image->iterations ?
              image->iterations-1 : 0));
            (void) WriteBlobByte(image,(unsigned char) 0x00);
          }
        if ((image->gamma != 1.0f/2.2f))
          {
            char
              attributes[MagickPathExtent];

            ssize_t
              count;

            /*
              Write ImageMagick extension.
            */
            (void) LogMagickEvent(CoderEvent,GetMagickModule(),
               "  Writing GIF Extension %s","ImageMagick");
            (void) WriteBlobByte(image,(unsigned char) 0x21);
            (void) WriteBlobByte(image,(unsigned char) 0xff);
            (void) WriteBlobByte(image,(unsigned char) 0x0b);
            (void) WriteBlob(image,11,(unsigned char *) "ImageMagick");
            count=FormatLocaleString(attributes,MagickPathExtent,"gamma=%g",
              image->gamma);
            (void) WriteBlobByte(image,(unsigned char) count);
            (void) WriteBlob(image,(size_t) count,(unsigned char *) attributes);
            (void) WriteBlobByte(image,(unsigned char) 0x00);
          }
        ResetImageProfileIterator(image);
        for ( ; ; )
        {
          char
            *name;

          const StringInfo
            *profile;

          name=GetNextImageProfile(image);
          if (name == (const char *) NULL)
            break;
          profile=GetImageProfile(image,name);
          if (profile != (StringInfo *) NULL)
          {
            if ((LocaleCompare(name,"ICC") == 0) ||
                (LocaleCompare(name,"ICM") == 0) ||
                (LocaleCompare(name,"IPTC") == 0) ||
                (LocaleCompare(name,"8BIM") == 0) ||
                (LocaleNCompare(name,"gif:",4) == 0))
            {
               ssize_t
                 offset;

               unsigned char
                 *datum;

               datum=GetStringInfoDatum(profile);
               length=GetStringInfoLength(profile);
               (void) WriteBlobByte(image,(unsigned char) 0x21);
               (void) WriteBlobByte(image,(unsigned char) 0xff);
               (void) WriteBlobByte(image,(unsigned char) 0x0b);
               if ((LocaleCompare(name,"ICC") == 0) ||
                   (LocaleCompare(name,"ICM") == 0))
                 {
                   /*
                     Write ICC extension.
                   */
                   (void) WriteBlob(image,11,(unsigned char *) "ICCRGBG1012");
                   (void) LogMagickEvent(CoderEvent,GetMagickModule(),
                     "  Writing GIF Extension %s","ICCRGBG1012");
                 }
               else
                 if ((LocaleCompare(name,"IPTC") == 0))
                   {
                     /*
                       Write IPTC extension.
                     */
                     (void) WriteBlob(image,11,(unsigned char *) "MGKIPTC0000");
                     (void) LogMagickEvent(CoderEvent,GetMagickModule(),
                       "  Writing GIF Extension %s","MGKIPTC0000");
                   }
                 else
                   if ((LocaleCompare(name,"8BIM") == 0))
                     {
                       /*
                         Write 8BIM extension.
                       */
                        (void) WriteBlob(image,11,(unsigned char *)
                          "MGK8BIM0000");
                        (void) LogMagickEvent(CoderEvent,GetMagickModule(),
                          "  Writing GIF Extension %s","MGK8BIM0000");
                     }
                   else
                     {
                       char
                         extension[MagickPathExtent];

                       /*
                         Write generic extension.
                       */
                       (void) CopyMagickString(extension,name+4,
                         sizeof(extension));
                       (void) WriteBlob(image,11,(unsigned char *) extension);
                       (void) LogMagickEvent(CoderEvent,GetMagickModule(),
                         "  Writing GIF Extension %s",name);
                     }
               offset=0;
               while ((ssize_t) length > offset)
               {
                 size_t
                   block_length;

                 if ((length-offset) < 255)
                   block_length=length-offset;
                 else
                   block_length=255;
                 (void) WriteBlobByte(image,(unsigned char) block_length);
                 (void) WriteBlob(image,(size_t) block_length,datum+offset);
                 offset+=(ssize_t) block_length;
               }
               (void) WriteBlobByte(image,(unsigned char) 0x00);
            }
          }
        }
      }
    (void) WriteBlobByte(image,',');  /* image separator */
    /*
      Write the image header.
    */
    page.x=image->page.x;
    page.y=image->page.y;
    if ((image->page.width != 0) && (image->page.height != 0))
      page=image->page;
    (void) WriteBlobLSBShort(image,(unsigned short) (page.x < 0 ? 0 : page.x));
    (void) WriteBlobLSBShort(image,(unsigned short) (page.y < 0 ? 0 : page.y));
    (void) WriteBlobLSBShort(image,(unsigned short) image->columns);
    (void) WriteBlobLSBShort(image,(unsigned short) image->rows);
    c=0x00;
    if (write_info->interlace != NoInterlace)
      c|=0x40;  /* pixel data is interlaced */
    for (j=0; j < (ssize_t) (3*image->colors); j++)
      if (colormap[j] != global_colormap[j])
        break;
    if (j == (ssize_t) (3*image->colors))
      (void) WriteBlobByte(image,(unsigned char) c);
    else
      {
        c|=0x80;
        c|=(bits_per_pixel-1);   /* size of local colormap */
        (void) WriteBlobByte(image,(unsigned char) c);
        length=(size_t) (3*(one << bits_per_pixel));
        (void) WriteBlob(image,length,colormap);
      }
    /*
      Write the image data.
    */
    c=(int) MagickMax(bits_per_pixel,2);
    (void) WriteBlobByte(image,(unsigned char) c);
    status=EncodeImage(write_info,image,(size_t) MagickMax(bits_per_pixel,2)+1,
      exception);
    if (status == MagickFalse)
      {
        global_colormap=(unsigned char *) RelinquishMagickMemory(
          global_colormap);
        colormap=(unsigned char *) RelinquishMagickMemory(colormap);
        write_info=DestroyImageInfo(write_info);
        ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
      }
    (void) WriteBlobByte(image,(unsigned char) 0x00);
    if (GetNextImageInList(image) == (Image *) NULL)
      break;
    image=SyncNextImageInList(image);
    scene++;
    status=SetImageProgress(image,SaveImagesTag,scene,
      GetImageListLength(image));
    if (status == MagickFalse)
      break;
  } while (write_info->adjoin != MagickFalse);
  (void) WriteBlobByte(image,';'); /* terminator */
  global_colormap=(unsigned char *) RelinquishMagickMemory(global_colormap);
  colormap=(unsigned char *) RelinquishMagickMemory(colormap);
  write_info=DestroyImageInfo(write_info);
  (void) CloseBlob(image);
  return(MagickTrue);
}