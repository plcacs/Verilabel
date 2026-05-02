void UTFstring::UpdateFromUTF8()
{
  delete [] _Data;
  // find the size of the final UCS-2 string
  size_t i;
  const size_t SrcLength = UTF8string.length();
  for (_Length=0, i=0; i<SrcLength; _Length++) {
    const unsigned int CharLength = UTFCharLength(static_cast<uint8>(UTF8string[i]));
    if ((CharLength >= 1) && (CharLength <= 4))
      i += CharLength;
    else
      // Invalid size?
      break;
  }
  _Data = new wchar_t[_Length+1];
  size_t j;
  for (j=0, i=0; i<SrcLength; j++) {
    const uint8 lead              = static_cast<uint8>(UTF8string[i]);
    const unsigned int CharLength = UTFCharLength(lead);
    if ((CharLength < 1) || (CharLength > 4))
      // Invalid char?
      break;

    if ((i + CharLength) > SrcLength)
      // Guard against invalid memory access beyond the end of the
      // source buffer.
      break;

    if (CharLength == 1)
      _Data[j] = lead;
    else if (CharLength == 2)
      _Data[j] = ((lead & 0x1F) << 6) + (UTF8string[i+1] & 0x3F);
    else if (CharLength == 3)
      _Data[j] = ((lead & 0x0F) << 12) + ((UTF8string[i+1] & 0x3F) << 6) + (UTF8string[i+2] & 0x3F);
    else if (CharLength == 4)
      _Data[j] = ((lead & 0x07) << 18) + ((UTF8string[i+1] & 0x3F) << 12) + ((UTF8string[i+2] & 0x3F) << 6) + (UTF8string[i+3] & 0x3F);

    i += CharLength;
  }
  _Data[j] = 0;
}