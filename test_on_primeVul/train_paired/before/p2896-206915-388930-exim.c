b64decode(const uschar *code, uschar **ptr)
{
int x, y;
uschar *result = store_get(3*(Ustrlen(code)/4) + 1);

*ptr = result;

/* Each cycle of the loop handles a quantum of 4 input bytes. For the last
quantum this may decode to 1, 2, or 3 output bytes. */

while ((x = *code++) != 0)
  {
  if (isspace(x)) continue;
  /* debug_printf("b64d: '%c'\n", x); */

  if (x > 127 || (x = dec64table[x]) == 255) return -1;

  while (isspace(y = *code++)) ;
  /* debug_printf("b64d: '%c'\n", y); */
  if (y == 0 || (y = dec64table[y]) == 255)
    return -1;

  *result++ = (x << 2) | (y >> 4);
  /* debug_printf("b64d:      -> %02x\n", result[-1]); */

  while (isspace(x = *code++)) ;
  /* debug_printf("b64d: '%c'\n", x); */
  if (x == '=')		/* endmarker, but there should be another */
    {
    while (isspace(x = *code++)) ;
    /* debug_printf("b64d: '%c'\n", x); */
    if (x != '=') return -1;
    while (isspace(y = *code++)) ;
    if (y != 0) return -1;
    /* debug_printf("b64d: DONE\n"); */
    break;
    }
  else
    {
    if (x > 127 || (x = dec64table[x]) == 255) return -1;
    *result++ = (y << 4) | (x >> 2);
    /* debug_printf("b64d:      -> %02x\n", result[-1]); */

    while (isspace(y = *code++)) ;
    /* debug_printf("b64d: '%c'\n", y); */
    if (y == '=')
      {
      while (isspace(y = *code++)) ;
      if (y != 0) return -1;
      /* debug_printf("b64d: DONE\n"); */
      break;
      }
    else
      {
      if (y > 127 || (y = dec64table[y]) == 255) return -1;
      *result++ = (x << 6) | y;
      /* debug_printf("b64d:      -> %02x\n", result[-1]); */
      }
    }
  }

*result = 0;
return result - *ptr;
}