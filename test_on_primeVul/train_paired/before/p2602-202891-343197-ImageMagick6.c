static StringInfo *ParseImageResourceBlocks(Image *image,
  const unsigned char *blocks,size_t length,
  MagickBooleanType *has_merged_image)
{
  const unsigned char
    *p;

  StringInfo
    *profile;

  unsigned char
    name_length;

  unsigned int
    count;

  unsigned short
    id,
    short_sans;

  if (length < 16)
    return((StringInfo *) NULL);
  profile=BlobToStringInfo((const void *) NULL,length);
  SetStringInfoDatum(profile,blocks);
  SetStringInfoName(profile,"8bim");
  for (p=blocks; (p >= blocks) && (p < (blocks+length-7)); )
  {
    if (LocaleNCompare((const char *) p,"8BIM",4) != 0)
      break;
    p+=4;
    p=PushShortPixel(MSBEndian,p,&id);
    p=PushCharPixel(p,&name_length);
    if (name_length % 2 == 0)
      name_length++;
    p+=name_length;
    if (p > (blocks+length-4))
      break;
    p=PushLongPixel(MSBEndian,p,&count);
    if ((p+count) > (blocks+length))
      break;
    switch (id)
    {
      case 0x03ed:
      {
        char
          value[MaxTextExtent];

        unsigned short
          resolution;

        /*
          Resolution info.
        */
        if (count < 16)
          break;
        p=PushShortPixel(MSBEndian,p,&resolution);
        image->x_resolution=(double) resolution;
        (void) FormatLocaleString(value,MaxTextExtent,"%g",
          image->x_resolution);
        (void) SetImageProperty(image,"tiff:XResolution",value);
        p=PushShortPixel(MSBEndian,p,&short_sans);
        p=PushShortPixel(MSBEndian,p,&short_sans);
        p=PushShortPixel(MSBEndian,p,&short_sans);
        p=PushShortPixel(MSBEndian,p,&resolution);
        image->y_resolution=(double) resolution;
        (void) FormatLocaleString(value,MaxTextExtent,"%g",
          image->y_resolution);
        (void) SetImageProperty(image,"tiff:YResolution",value);
        p=PushShortPixel(MSBEndian,p,&short_sans);
        p=PushShortPixel(MSBEndian,p,&short_sans);
        p=PushShortPixel(MSBEndian,p,&short_sans);
        image->units=PixelsPerInchResolution;
        break;
      }
      case 0x0421:
      {
        if ((count > 4) && (*(p+4) == 0))
          *has_merged_image=MagickFalse;
        p+=count;
        break;
      }
      default:
      {
        p+=count;
        break;
      }
    }
    if ((count & 0x01) != 0)
      p++;
  }
  return(profile);
}