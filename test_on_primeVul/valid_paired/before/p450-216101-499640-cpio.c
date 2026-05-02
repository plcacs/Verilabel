ds_fgetstr (FILE *f, dynamic_string *s, char eos)
{
  int insize;			/* Amount needed for line.  */
  int strsize;			/* Amount allocated for S.  */
  int next_ch;

  /* Initialize.  */
  insize = 0;
  strsize = s->ds_length;

  /* Read the input string.  */
  next_ch = getc (f);
  while (next_ch != eos && next_ch != EOF)
    {
      if (insize >= strsize - 1)
	{
	  ds_resize (s, strsize * 2 + 2);
	  strsize = s->ds_length;
	}
      s->ds_string[insize++] = next_ch;
      next_ch = getc (f);
    }
  s->ds_string[insize++] = '\0';

  if (insize == 1 && next_ch == EOF)
    return NULL;
  else
    return s->ds_string;
}