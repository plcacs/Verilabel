LZWReadByte (FILE *fd,
             gint  just_reset_LZW,
             gint  input_code_size)
{
  static gint fresh = FALSE;
  gint        code, incode;
  static gint code_size, set_code_size;
  static gint max_code, max_code_size;
  static gint firstcode, oldcode;
  static gint clear_code, end_code;
  static gint table[2][(1 << MAX_LZW_BITS)];
  static gint stack[(1 << (MAX_LZW_BITS)) * 2], *sp;
  gint        i;

  if (just_reset_LZW)
    {
      if (input_code_size > MAX_LZW_BITS)
        {
          g_message ("Value out of range for code size (corrupted file?)");
          return -1;
        }

      set_code_size = input_code_size;
      code_size     = set_code_size + 1;
      clear_code    = 1 << set_code_size;
      end_code      = clear_code + 1;
      max_code_size = 2 * clear_code;
      max_code      = clear_code + 2;

      GetCode (fd, 0, TRUE);

      fresh = TRUE;

      sp = stack;

      for (i = 0; i < clear_code; ++i)
        {
          table[0][i] = 0;
          table[1][i] = i;
        }
      for (; i < (1 << MAX_LZW_BITS); ++i)
        {
          table[0][i] = 0;
          table[1][i] = 0;
        }

      return 0;
    }
  else if (fresh)
    {
      fresh = FALSE;
      do
        {
          firstcode = oldcode = GetCode (fd, code_size, FALSE);
        }
      while (firstcode == clear_code);

      return firstcode & 255;
    }

  if (sp > stack)
    return (*--sp) & 255;

  while ((code = GetCode (fd, code_size, FALSE)) >= 0)
    {
      if (code == clear_code)
        {
          for (i = 0; i < clear_code; ++i)
            {
              table[0][i] = 0;
              table[1][i] = i;
            }
          for (; i < (1 << MAX_LZW_BITS); ++i)
            {
              table[0][i] = 0;
              table[1][i] = 0;
            }

          code_size     = set_code_size + 1;
          max_code_size = 2 * clear_code;
          max_code      = clear_code + 2;
          sp            = stack;
          firstcode     = oldcode = GetCode (fd, code_size, FALSE);

          return firstcode & 255;
        }
      else if (code == end_code)
        {
          gint   count;
          guchar buf[260];

          if (ZeroDataBlock)
            return -2;

          while ((count = GetDataBlock (fd, buf)) > 0)
            ;

          if (count != 0)
            g_print ("GIF: missing EOD in data stream (common occurence)");

          return -2;
        }

      incode = code;

      if (code >= max_code)
        {
          *sp++ = firstcode;
          code = oldcode;
        }

      while (code >= clear_code)
        {
          *sp++ = table[1][code];
          if (code == table[0][code])
            {
              g_message ("Circular table entry.  Corrupt file.");
              gimp_quit ();
            }
          code = table[0][code];
        }

      *sp++ = firstcode = table[1][code];

      if ((code = max_code) < (1 << MAX_LZW_BITS))
        {
          table[0][code] = oldcode;
          table[1][code] = firstcode;
          ++max_code;
          if ((max_code >= max_code_size) &&
              (max_code_size < (1 << MAX_LZW_BITS)))
            {
              max_code_size *= 2;
              ++code_size;
            }
        }

      oldcode = incode;

      if (sp > stack)
        return (*--sp) & 255;
    }

  return code & 255;
}