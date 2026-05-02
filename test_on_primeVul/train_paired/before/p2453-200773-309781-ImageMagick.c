static MagickBooleanType WriteCIPImage(const ImageInfo *image_info,Image *image,
  ExceptionInfo *exception)
{
  char
    buffer[MagickPathExtent];

  const char
    *value;

  MagickBooleanType
    status;

  register const Quantum
    *p;

  register ssize_t
    i,
    x;

  ssize_t
    y;

  unsigned char
    byte;

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
  (void) WriteBlobString(image,"<CiscoIPPhoneImage>\n");
  value=GetImageProperty(image,"label",exception);
  if (value != (const char *) NULL)
    (void) FormatLocaleString(buffer,MagickPathExtent,"<Title>%s</Title>\n",value);
  else
    {
      char
        basename[MagickPathExtent];

      GetPathComponent(image->filename,BasePath,basename);
      (void) FormatLocaleString(buffer,MagickPathExtent,"<Title>%s</Title>\n",
        basename);
    }
  (void) WriteBlobString(image,buffer);
  (void) FormatLocaleString(buffer,MagickPathExtent,
    "<LocationX>%.20g</LocationX>\n",(double) image->page.x);
  (void) WriteBlobString(image,buffer);
  (void) FormatLocaleString(buffer,MagickPathExtent,
    "<LocationY>%.20g</LocationY>\n",(double) image->page.y);
  (void) WriteBlobString(image,buffer);
  (void) FormatLocaleString(buffer,MagickPathExtent,"<Width>%.20g</Width>\n",
    (double) (image->columns+(image->columns % 2)));
  (void) WriteBlobString(image,buffer);
  (void) FormatLocaleString(buffer,MagickPathExtent,"<Height>%.20g</Height>\n",
    (double) image->rows);
  (void) WriteBlobString(image,buffer);
  (void) FormatLocaleString(buffer,MagickPathExtent,"<Depth>2</Depth>\n");
  (void) WriteBlobString(image,buffer);
  (void) WriteBlobString(image,"<Data>");
  (void) TransformImageColorspace(image,sRGBColorspace,exception);
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    p=GetVirtualPixels(image,0,y,image->columns,1,exception);
    if (p == (const Quantum *) NULL)
      break;
    for (x=0; x < ((ssize_t) image->columns-3); x+=4)
    {
      byte=(unsigned char)
        ((((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+3*GetPixelChannels(image)))/QuantumRange) & 0x03) << 6) |
         (((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+2*GetPixelChannels(image)))/QuantumRange) & 0x03) << 4) |
         (((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+1*GetPixelChannels(image)))/QuantumRange) & 0x03) << 2) |
         (((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+0*GetPixelChannels(image)))/QuantumRange) & 0x03) << 0));
      (void) FormatLocaleString(buffer,MagickPathExtent,"%02x",byte);
      (void) WriteBlobString(image,buffer);
      p+=GetPixelChannels(image);
    }
    if ((image->columns % 4) != 0)
      {
        i=(ssize_t) image->columns % 4;
        byte=(unsigned char)
          ((((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+MagickMin(i,3)*GetPixelChannels(image)))/QuantumRange) & 0x03) << 6) |
           (((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+MagickMin(i,2)*GetPixelChannels(image)))/QuantumRange) & 0x03) << 4) |
           (((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+MagickMin(i,1)*GetPixelChannels(image)))/QuantumRange) & 0x03) << 2) |
           (((size_t) (3*ClampToQuantum(GetPixelLuma(image,p+MagickMin(i,0)*GetPixelChannels(image)))/QuantumRange) & 0x03) << 0));
        (void) FormatLocaleString(buffer,MagickPathExtent,"%02x",~byte);
        (void) WriteBlobString(image,buffer);
      }
    status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      break;
  }
  (void) WriteBlobString(image,"</Data>\n");
  (void) WriteBlobString(image,"</CiscoIPPhoneImage>\n");
  (void) CloseBlob(image);
  return(MagickTrue);
}