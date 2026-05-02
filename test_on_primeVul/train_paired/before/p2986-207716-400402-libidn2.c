idn2_to_ascii_4i (const uint32_t * input, size_t inlen, char * output, int flags)
{
  uint32_t *input_u32;
  uint8_t *input_u8, *output_u8;
  size_t length;
  int rc;

  if (!input)
    {
      if (output)
	*output = 0;
      return IDN2_OK;
    }

  input_u32 = (uint32_t *) malloc ((inlen + 1) * sizeof(uint32_t));
  if (!input_u32)
    return IDN2_MALLOC;

  u32_cpy (input_u32, input, inlen);
  input_u32[inlen] = 0;

  input_u8 = u32_to_u8 (input_u32, inlen + 1, NULL, &length);
  free (input_u32);
  if (!input_u8)
    {
      if (errno == ENOMEM)
	return IDN2_MALLOC;
      return IDN2_ENCODING_ERROR;
    }

  rc = idn2_lookup_u8 (input_u8, &output_u8, flags);
  free (input_u8);

  if (rc == IDN2_OK)
    {
      /* wow, this is ugly, but libidn manpage states:
       * char * out  output zero terminated string that must have room for at
       * least 63 characters plus the terminating zero.
       */
      if (output)
	strcpy (output, (const char *) output_u8);

      free(output_u8);
    }

  return rc;
}