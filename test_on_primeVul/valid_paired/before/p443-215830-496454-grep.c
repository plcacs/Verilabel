grep (int fd, char const *file, struct stats *stats)
{
  int nlines, i;
  int not_text;
  size_t residue, save;
  char oldc;
  char *beg;
  char *lim;
  char eol = eolbyte;

  if (!reset (fd, file, stats))
    return 0;

  if (file && directories == RECURSE_DIRECTORIES
      && S_ISDIR (stats->stat.st_mode))
    {
      /* Close fd now, so that we don't open a lot of file descriptors
         when we recurse deeply.  */
      if (close (fd) != 0)
        suppressible_error (file, errno);
      return grepdir (file, stats) - 2;
    }

  totalcc = 0;
  lastout = 0;
  totalnl = 0;
  outleft = max_count;
  after_last_match = 0;
  pending = 0;

  nlines = 0;
  residue = 0;
  save = 0;

  if (! fillbuf (save, stats))
    {
      suppressible_error (filename, errno);
      return 0;
    }

  not_text = (((binary_files == BINARY_BINARY_FILES && !out_quiet)
               || binary_files == WITHOUT_MATCH_BINARY_FILES)
              && memchr (bufbeg, eol ? '\0' : '\200', buflim - bufbeg));
  if (not_text && binary_files == WITHOUT_MATCH_BINARY_FILES)
    return 0;
  done_on_match += not_text;
  out_quiet += not_text;

  for (;;)
    {
      lastnl = bufbeg;
      if (lastout)
        lastout = bufbeg;

      beg = bufbeg + save;

      /* no more data to scan (eof) except for maybe a residue -> break */
      if (beg == buflim)
        break;

      /* Determine new residue (the length of an incomplete line at the end of
         the buffer, 0 means there is no incomplete last line).  */
      oldc = beg[-1];
      beg[-1] = eol;
      for (lim = buflim; lim[-1] != eol; lim--)
        continue;
      beg[-1] = oldc;
      if (lim == beg)
        lim = beg - residue;
      beg -= residue;
      residue = buflim - lim;

      if (beg < lim)
        {
          if (outleft)
            nlines += grepbuf (beg, lim);
          if (pending)
            prpending (lim);
          if ((!outleft && !pending) || (nlines && done_on_match && !out_invert))
            goto finish_grep;
        }

      /* The last OUT_BEFORE lines at the end of the buffer will be needed as
         leading context if there is a matching line at the begin of the
         next data. Make beg point to their begin.  */
      i = 0;
      beg = lim;
      while (i < out_before && beg > bufbeg && beg != lastout)
        {
          ++i;
          do
            --beg;
          while (beg[-1] != eol);
        }

      /* detect if leading context is discontinuous from last printed line.  */
      if (beg != lastout)
        lastout = 0;

      /* Handle some details and read more data to scan.  */
      save = residue + lim - beg;
      if (out_byte)
        totalcc = add_count (totalcc, buflim - bufbeg - save);
      if (out_line)
        nlscan (beg);
      if (! fillbuf (save, stats))
        {
          suppressible_error (filename, errno);
          goto finish_grep;
        }
    }
  if (residue)
    {
      *buflim++ = eol;
      if (outleft)
        nlines += grepbuf (bufbeg + save - residue, buflim);
      if (pending)
        prpending (buflim);
    }

 finish_grep:
  done_on_match -= not_text;
  out_quiet -= not_text;
  if ((not_text & ~out_quiet) && nlines != 0)
    printf (_("Binary file %s matches\n"), filename);
  return nlines;
}