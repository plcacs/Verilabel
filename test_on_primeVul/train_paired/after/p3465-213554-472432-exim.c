spa_base64_to_bits (char *out, int outlength, const char *in)
/* base 64 to raw bytes in quasi-big-endian order, returning count of bytes */
{
int len = 0;
uschar digit1, digit2, digit3, digit4;

if (in[0] == '+' && in[1] == ' ')
  in += 2;
if (*in == '\r')
  return (0);

do
  {
  if (len >= outlength)                   /* Added by PH */
    return -1;                          /* Added by PH */
  digit1 = in[0];
  if (DECODE64 (digit1) == BAD)
    return -1;
  digit2 = in[1];
  if (DECODE64 (digit2) == BAD)
    return -1;
  digit3 = in[2];
  if (digit3 != '=' && DECODE64 (digit3) == BAD)
    return -1;
  digit4 = in[3];
  if (digit4 != '=' && DECODE64 (digit4) == BAD)
    return -1;
  in += 4;
  *out++ = (DECODE64 (digit1) << 2) | (DECODE64 (digit2) >> 4);
  ++len;
  if (digit3 != '=')
    {
    if (len >= outlength)                   /* Added by PH */
      return -1;                          /* Added by PH */
    *out++ =
      ((DECODE64 (digit2) << 4) & 0xf0) | (DECODE64 (digit3) >> 2);
    ++len;
    if (digit4 != '=')
      {
      if (len >= outlength)                   /* Added by PH */
	return -1;                          /* Added by PH */
      *out++ = ((DECODE64 (digit3) << 6) & 0xc0) | DECODE64 (digit4);
      ++len;
      }
    }
  }
while (*in && *in != '\r' && digit4 != '=');

return len;
}