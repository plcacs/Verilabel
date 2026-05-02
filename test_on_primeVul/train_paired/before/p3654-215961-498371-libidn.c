idna_to_ascii_4i (const uint32_t * in, size_t inlen, char *out, int flags)
{
  size_t len, outlen;
  uint32_t *src;		/* XXX don't need to copy data? */
  int rc;

  /*
   * ToASCII consists of the following steps:
   *
   * 1. If all code points in the sequence are in the ASCII range (0..7F)
   * then skip to step 3.
   */

  {
    size_t i;
    int inasciirange;

    inasciirange = 1;
    for (i = 0; i < inlen; i++)
      if (in[i] > 0x7F)
	inasciirange = 0;
    if (inasciirange)
      {
	src = malloc (sizeof (in[0]) * (inlen + 1));
	if (src == NULL)
	  return IDNA_MALLOC_ERROR;

	memcpy (src, in, sizeof (in[0]) * inlen);
	src[inlen] = 0;

	goto step3;
      }
  }

  /*
   * 2. Perform the steps specified in [NAMEPREP] and fail if there is
   * an error. The AllowUnassigned flag is used in [NAMEPREP].
   */

  {
    char *p;

    p = stringprep_ucs4_to_utf8 (in, (ssize_t) inlen, NULL, NULL);
    if (p == NULL)
      return IDNA_MALLOC_ERROR;

    len = strlen (p);
    do
      {
	char *newp;

	len = 2 * len + 10;	/* XXX better guess? */
	newp = realloc (p, len);
	if (newp == NULL)
	  {
	    free (p);
	    return IDNA_MALLOC_ERROR;
	  }
	p = newp;

	if (flags & IDNA_ALLOW_UNASSIGNED)
	  rc = stringprep_nameprep (p, len);
	else
	  rc = stringprep_nameprep_no_unassigned (p, len);
      }
    while (rc == STRINGPREP_TOO_SMALL_BUFFER);

    if (rc != STRINGPREP_OK)
      {
	free (p);
	return IDNA_STRINGPREP_ERROR;
      }

    src = stringprep_utf8_to_ucs4 (p, -1, NULL);

    free (p);

    if (!src)
      return IDNA_MALLOC_ERROR;
  }

step3:
  /*
   * 3. If the UseSTD3ASCIIRules flag is set, then perform these checks:
   *
   * (a) Verify the absence of non-LDH ASCII code points; that is,
   * the absence of 0..2C, 2E..2F, 3A..40, 5B..60, and 7B..7F.
   *
   * (b) Verify the absence of leading and trailing hyphen-minus;
   * that is, the absence of U+002D at the beginning and end of
   * the sequence.
   */

  if (flags & IDNA_USE_STD3_ASCII_RULES)
    {
      size_t i;

      for (i = 0; src[i]; i++)
	if (src[i] <= 0x2C || src[i] == 0x2E || src[i] == 0x2F ||
	    (src[i] >= 0x3A && src[i] <= 0x40) ||
	    (src[i] >= 0x5B && src[i] <= 0x60) ||
	    (src[i] >= 0x7B && src[i] <= 0x7F))
	  {
	    free (src);
	    return IDNA_CONTAINS_NON_LDH;
	  }

      if (src[0] == 0x002D || (i > 0 && src[i - 1] == 0x002D))
	{
	  free (src);
	  return IDNA_CONTAINS_MINUS;
	}
    }

  /*
   * 4. If all code points in the sequence are in the ASCII range
   * (0..7F), then skip to step 8.
   */

  {
    size_t i;
    int inasciirange;

    inasciirange = 1;
    for (i = 0; src[i]; i++)
      {
	if (src[i] > 0x7F)
	  inasciirange = 0;
	/* copy string to output buffer if we are about to skip to step8 */
	if (i < 64)
	  out[i] = src[i];
      }
    if (i < 64)
      out[i] = '\0';
    else
      return IDNA_INVALID_LENGTH;
    if (inasciirange)
      goto step8;
  }

  /*
   * 5. Verify that the sequence does NOT begin with the ACE prefix.
   *
   */

  {
    size_t i;
    int match;

    match = 1;
    for (i = 0; match && i < strlen (IDNA_ACE_PREFIX); i++)
      if (((uint32_t) IDNA_ACE_PREFIX[i] & 0xFF) != src[i])
	match = 0;
    if (match)
      {
	free (src);
	return IDNA_CONTAINS_ACE_PREFIX;
      }
  }

  /*
   * 6. Encode the sequence using the encoding algorithm in [PUNYCODE]
   * and fail if there is an error.
   */
  for (len = 0; src[len]; len++)
    ;
  src[len] = '\0';
  outlen = 63 - strlen (IDNA_ACE_PREFIX);
  rc = punycode_encode (len, src, NULL,
			&outlen, &out[strlen (IDNA_ACE_PREFIX)]);
  if (rc != PUNYCODE_SUCCESS)
    {
      free (src);
      return IDNA_PUNYCODE_ERROR;
    }
  out[strlen (IDNA_ACE_PREFIX) + outlen] = '\0';

  /*
   * 7. Prepend the ACE prefix.
   */

  memcpy (out, IDNA_ACE_PREFIX, strlen (IDNA_ACE_PREFIX));

  /*
   * 8. Verify that the number of code points is in the range 1 to 63
   * inclusive (0 is excluded).
   */

step8:
  free (src);
  if (strlen (out) < 1)
    return IDNA_INVALID_LENGTH;

  return IDNA_SUCCESS;
}