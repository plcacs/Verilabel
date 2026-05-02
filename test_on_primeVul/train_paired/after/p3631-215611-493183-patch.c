strip_leading_slashes (char *name, int strip_leading)
{
  int s = strip_leading;
  char *p, *n;

  for (p = n = name;  *p;  p++)
    {
      if (ISSLASH (*p))
	{
	  while (ISSLASH (p[1]))
	    p++;
	  if (strip_leading < 0 || --s >= 0)
	      n = p+1;
	}
    }
  if (IS_ABSOLUTE_FILE_NAME (n))
    fatal ("rejecting absolute file name: %s", quotearg (n));
  for (p = n; *p; )
    {
      if (*p == '.' && *++p == '.' && ( ! *++p || ISSLASH (*p)))
	fatal ("rejecting file name with \"..\" component: %s", quotearg (n));
      while (*p && ! ISSLASH (*p))
	p++;
      while (ISSLASH (*p))
	p++;
    }
  if ((strip_leading < 0 || s <= 0) && *n)
    {
      memmove (name, n, strlen (n) + 1);
      return true;
    }
  else
    return false;
}