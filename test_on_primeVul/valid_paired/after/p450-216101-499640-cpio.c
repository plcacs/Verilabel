ds_fgetstr (FILE *f, dynamic_string *s, char eos)
{
  int next_ch;

  /* Initialize.  */
  s->ds_idx = 0;

  /* Read the input string.  */
  while ((next_ch = getc (f)) != eos && next_ch != EOF)
    {
      ds_resize (s);
      s->ds_string[s->ds_idx++] = next_ch;
    }
  ds_resize (s);
  s->ds_string[s->ds_idx] = '\0';

  if (s->ds_idx == 0 && next_ch == EOF)
    return NULL;
  else
    return s->ds_string;
}