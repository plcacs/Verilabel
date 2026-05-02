static unsigned decode_digit(int cp)
{
  return (unsigned) (cp - 48 < 10 ? cp - 22 :  cp - 65 < 26 ? cp - 65 :
         cp - 97 < 26 ? cp - 97 :  base);
}