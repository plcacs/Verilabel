void CLASS panasonic_load_raw()
{
  int row, col, i, j, sh = 0, pred[2], nonz[2];

  pana_bits(0);
  for (row = 0; row < height; row++)
  {
#ifdef LIBRAW_LIBRARY_BUILD
    checkCancel();
#endif
    for (col = 0; col < raw_width; col++)
    {
      if ((i = col % 14) == 0)
        pred[0] = pred[1] = nonz[0] = nonz[1] = 0;
      if (i % 3 == 2)
        sh = 4 >> (3 - pana_bits(2));
      if (nonz[i & 1])
      {
        if ((j = pana_bits(8)))
        {
          if ((pred[i & 1] -= 0x80 << sh) < 0 || sh == 4)
            pred[i & 1] &= ~((~0u) << sh);
          pred[i & 1] += j << sh;
        }
      }
      else if ((nonz[i & 1] = pana_bits(8)) || i > 11)
        pred[i & 1] = nonz[i & 1] << 4 | pana_bits(4);
      if ((RAW(row, col) = pred[col & 1]) > 4098 && col < width)
        derror();
    }
  }
}