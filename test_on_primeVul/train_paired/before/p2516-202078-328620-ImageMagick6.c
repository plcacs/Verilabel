static void SVGStripString(const MagickBooleanType trim,char *message)
{
  register char
    *p,
    *q;

  size_t
    length;

  assert(message != (char *) NULL);
  if (*message == '\0')
    return;
  /*
    Remove comment.
  */
  q=message;
  for (p=message; *p != '\0'; p++)
  {
    if ((*p == '/') && (*(p+1) == '*'))
      {
        for ( ; *p != '\0'; p++)
          if ((*p == '*') && (*(p+1) == '/'))
            break;
        if (*p == '\0')
          break;
        p+=2;
      }
    *q++=(*p);
  }
  *q='\0';
  if (trim != MagickFalse)
    {
      /*
        Remove whitespace.
      */
      length=strlen(message);
      p=message;
      while (isspace((int) ((unsigned char) *p)) != 0)
        p++;
      if ((*p == '\'') || (*p == '"'))
        p++;
      q=message+length-1;
      while ((isspace((int) ((unsigned char) *q)) != 0) && (q > p))
        q--;
      if (q > p)
        if ((*q == '\'') || (*q == '"'))
          q--;
      (void) memmove(message,p,(size_t) (q-p+1));
      message[q-p+1]='\0';
    }
  /*
    Convert newlines to a space.
  */
  for (p=message; *p != '\0'; p++)
    if (*p == '\n')
      *p=' ';
}