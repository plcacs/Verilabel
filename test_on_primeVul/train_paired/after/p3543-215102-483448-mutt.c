static void mutt_decode_uuencoded (STATE *s, LOFF_T len, int istext, iconv_t cd)
{
  char tmps[SHORT_STRING];
  char linelen, c, l, out;
  char *pt;
  char bufi[BUFI_SIZE];
  size_t k = 0;

  if (istext)
    state_set_prefix(s);

  while (len > 0)
  {
    if ((fgets(tmps, sizeof(tmps), s->fpin)) == NULL)
      return;
    len -= mutt_strlen(tmps);
    if ((!mutt_strncmp (tmps, "begin", 5)) && ISSPACE (tmps[5]))
      break;
  }
  while (len > 0)
  {
    if ((fgets(tmps, sizeof(tmps), s->fpin)) == NULL)
      return;
    len -= mutt_strlen(tmps);
    if (!mutt_strncmp (tmps, "end", 3))
      break;
    pt = tmps;
    linelen = decode_byte (*pt);
    pt++;
    for (c = 0; c < linelen && *pt;)
    {
      for (l = 2; l <= 6 && *pt && *(pt + 1); l += 2)
      {
	out = decode_byte (*pt) << l;
	pt++;
	out |= (decode_byte (*pt) >> (6 - l));
	bufi[k++] = out;
	c++;
	if (c == linelen)
	  break;
      }
      mutt_convert_to_state (cd, bufi, &k, s);
      pt++;
    }
  }

  mutt_convert_to_state (cd, bufi, &k, s);
  mutt_convert_to_state (cd, 0, 0, s);

  state_reset_prefix(s);
}