void UTFstring::UpdateFromUTF8()
{
  delete [] _Data;
  // find the size of the final UCS-2 string
  size_t i;
  for (_Length=0, i=0; i<UTF8string.length(); _Length++) {
    uint8 lead = static_cast<uint8>(UTF8string[i]);
    if (lead < 0x80)
      i++;
    else if ((lead >> 5) == 0x6)
      i += 2;
    else if ((lead >> 4) == 0xe)
      i += 3;
    else if ((lead >> 3) == 0x1e)
      i += 4;
    else
      // Invalid size?
      break;
  }
  _Data = new wchar_t[_Length+1];
  size_t j;
  for (j=0, i=0; i<UTF8string.length(); j++) {
    uint8 lead = static_cast<uint8>(UTF8string[i]);
    if (lead < 0x80) {
      _Data[j] = lead;
      i++;
    } else if ((lead >> 5) == 0x6) {
      _Data[j] = ((lead & 0x1F) << 6) + (UTF8string[i+1] & 0x3F);
      i += 2;
    } else if ((lead >> 4) == 0xe) {
      _Data[j] = ((lead & 0x0F) << 12) + ((UTF8string[i+1] & 0x3F) << 6) + (UTF8string[i+2] & 0x3F);
      i += 3;
    } else if ((lead >> 3) == 0x1e) {
      _Data[j] = ((lead & 0x07) << 18) + ((UTF8string[i+1] & 0x3F) << 12) + ((UTF8string[i+2] & 0x3F) << 6) + (UTF8string[i+3] & 0x3F);
      i += 4;
    } else
      // Invalid char?
      break;
  }
  _Data[j] = 0;
}