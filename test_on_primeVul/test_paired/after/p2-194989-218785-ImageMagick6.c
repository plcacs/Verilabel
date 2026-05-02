static MagickBooleanType ReadPSDChannelPixels(Image *image,
  const size_t channels,const ssize_t row,const ssize_t type,
  const unsigned char *pixels,ExceptionInfo *exception)
{
  Quantum
    pixel;

  const unsigned char
    *p;

  IndexPacket
    *indexes;

  PixelPacket
    *q;

  ssize_t
    x;

  size_t
    packet_size;

  unsigned short
    nibble;

  p=pixels;
  q=GetAuthenticPixels(image,0,row,image->columns,1,exception);
  if (q == (PixelPacket *) NULL)
    return MagickFalse;
  indexes=GetAuthenticIndexQueue(image);
  packet_size=GetPSDPacketSize(image);
  for (x=0; x < (ssize_t) image->columns; x++)
  {
    if (packet_size == 1)
      pixel=ScaleCharToQuantum(*p++);
    else
      if (packet_size == 2)
        {
          p=PushShortPixel(MSBEndian,p,&nibble);
          pixel=ScaleShortToQuantum(nibble);
        }
      else
        {
          MagickFloatType
            nibble;

          p=PushFloatPixel(MSBEndian,p,&nibble);
          pixel=ClampToQuantum((MagickRealType)QuantumRange*nibble);
        }
    if (image->depth > 1)
      {
        SetPSDPixel(image,channels,type,packet_size,pixel,q,indexes,x);
        q++;
      }
    else
      {
        ssize_t
          bit,
          number_bits;

        number_bits=(ssize_t) image->columns-x;
        if (number_bits > 8)
          number_bits=8;
        for (bit=0; bit < number_bits; bit++)
        {
          SetPSDPixel(image,channels,type,packet_size,
            (((unsigned char) ((ssize_t) pixel)) & (0x01 << (7-bit))) != 0 ? 0 :
            QuantumRange,q++,indexes,x++);
        }
        if (x != (ssize_t) image->columns)
          x--;
        continue;
      }
  }
  return(SyncAuthenticPixels(image,exception));
}