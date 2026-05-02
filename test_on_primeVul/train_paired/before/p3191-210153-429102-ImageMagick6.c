static void ExportIndexQuantum(const Image *image,QuantumInfo *quantum_info,
  const MagickSizeType number_pixels,const PixelPacket *magick_restrict p,
  const IndexPacket *magick_restrict indexes,unsigned char *magick_restrict q,
  ExceptionInfo *exception)
{
  ssize_t
    x;

  ssize_t
    bit;

  if (image->storage_class != PseudoClass)
    {
      (void) ThrowMagickException(exception,GetMagickModule(),ImageError,
        "ColormappedImageRequired","`%s'",image->filename);
      return;
    }
  switch (quantum_info->depth)
  {
    case 1:
    {
      unsigned char
        pixel;

      for (x=((ssize_t) number_pixels-7); x > 0; x-=8)
      {
        pixel=(unsigned char) *indexes++;
        *q=((pixel & 0x01) << 7);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0x01) << 6);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0x01) << 5);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0x01) << 4);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0x01) << 3);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0x01) << 2);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0x01) << 1);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0x01) << 0);
        q++;
      }
      if ((number_pixels % 8) != 0)
        {
          *q='\0';
          for (bit=7; bit >= (ssize_t) (8-(number_pixels % 8)); bit--)
          {
            pixel=(unsigned char) *indexes++;
            *q|=((pixel & 0x01) << (unsigned char) bit);
          }
          q++;
        }
      break;
    }
    case 4:
    {
      unsigned char
        pixel;

      for (x=0; x < (ssize_t) (number_pixels-1) ; x+=2)
      {
        pixel=(unsigned char) *indexes++;
        *q=((pixel & 0xf) << 4);
        pixel=(unsigned char) *indexes++;
        *q|=((pixel & 0xf) << 0);
        q++;
      }
      if ((number_pixels % 2) != 0)
        {
          pixel=(unsigned char) *indexes++;
          *q=((pixel & 0xf) << 4);
          q++;
        }
      break;
    }
    case 8:
    {
      for (x=0; x < (ssize_t) number_pixels; x++)
      {
        q=PopCharPixel((unsigned char) GetPixelIndex(indexes+x),q);
        q+=quantum_info->pad;
      }
      break;
    }
    case 16:
    {
      if (quantum_info->format == FloatingPointQuantumFormat)
        {
          for (x=0; x < (ssize_t) number_pixels; x++)
          {
            q=PopShortPixel(quantum_info->endian,SinglePrecisionToHalf(QuantumScale*
              GetPixelIndex(indexes+x)),q);
            q+=quantum_info->pad;
          }
          break;
        }
      for (x=0; x < (ssize_t) number_pixels; x++)
      {
        q=PopShortPixel(quantum_info->endian,(unsigned short) GetPixelIndex(indexes+x),q);
        q+=quantum_info->pad;
      }
      break;
    }
    case 32:
    {
      if (quantum_info->format == FloatingPointQuantumFormat)
        {
          for (x=0; x < (ssize_t) number_pixels; x++)
          {
            q=PopFloatPixel(quantum_info,(float) GetPixelIndex(indexes+x),q);
            p++;
            q+=quantum_info->pad;
          }
          break;
        }
      for (x=0; x < (ssize_t) number_pixels; x++)
      {
        q=PopLongPixel(quantum_info->endian,(unsigned int) GetPixelIndex(indexes+x),q);
        q+=quantum_info->pad;
      }
      break;
    }
    case 64:
    {
      if (quantum_info->format == FloatingPointQuantumFormat)
        {
          for (x=0; x < (ssize_t) number_pixels; x++)
          {
            q=PopDoublePixel(quantum_info,(double) GetPixelIndex(indexes+x),
              q);
            p++;
            q+=quantum_info->pad;
          }
          break;
        }
    }
    default:
    {
      for (x=0; x < (ssize_t) number_pixels; x++)
      {
        q=PopQuantumPixel(quantum_info,
          GetPixelIndex(indexes+x),q);
        p++;
        q+=quantum_info->pad;
      }
      break;
    }
  }
}