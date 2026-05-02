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
  if ((strip_leading < 0 || s <= 0) && *n)
    {
      memmove (name, n, strlen (n) + 1);
      return true;
    }
  else
    return false;
}