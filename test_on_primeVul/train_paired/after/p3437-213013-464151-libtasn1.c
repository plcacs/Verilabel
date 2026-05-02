_asn1_decode_simple_ber (unsigned int etype, const unsigned char *der,
			unsigned int _der_len, unsigned char **str,
			unsigned int *str_len, unsigned int *ber_len,
			unsigned dflags)
{
  int tag_len, len_len;
  const unsigned char *p;
  int der_len = _der_len;
  uint8_t *total = NULL;
  unsigned total_size = 0;
  unsigned char class;
  unsigned long tag;
  unsigned char *out = NULL;
  const unsigned char *cout = NULL;
  unsigned out_len;
  long result;

  if (ber_len) *ber_len = 0;

  if (der == NULL || der_len == 0)
    {
      warn();
      return ASN1_VALUE_NOT_VALID;
    }

  if (ETYPE_OK (etype) == 0)
    {
      warn();
      return ASN1_VALUE_NOT_VALID;
    }

  /* doesn't handle constructed + definite classes */
  class = ETYPE_CLASS (etype);
  if (class != ASN1_CLASS_UNIVERSAL)
    {
      warn();
      return ASN1_VALUE_NOT_VALID;
    }

  p = der;

  if (dflags & DECODE_FLAG_HAVE_TAG)
    {
      result = asn1_get_tag_der (p, der_len, &class, &tag_len, &tag);
        if (result != ASN1_SUCCESS)
          {
            warn();
            return result;
          }

        if (tag != ETYPE_TAG (etype))
          {
            warn();
            return ASN1_DER_ERROR;
          }

        p += tag_len;

        DECR_LEN(der_len, tag_len);

        if (ber_len) *ber_len += tag_len;
    }

  /* indefinite constructed */
  if ((((dflags & DECODE_FLAG_INDEFINITE) || class == ASN1_CLASS_STRUCTURED) && ETYPE_IS_STRING(etype)) &&
      !(dflags & DECODE_FLAG_LEVEL3))
    {
      len_len = 1;

      DECR_LEN(der_len, len_len);
      if (p[0] != 0x80)
        {
          warn();
          result = ASN1_DER_ERROR;
          goto cleanup;
        }

      p += len_len;

      if (ber_len) *ber_len += len_len;

      /* decode the available octet strings */
      do
        {
          unsigned tmp_len;
          unsigned flags = DECODE_FLAG_HAVE_TAG;

          if (dflags & DECODE_FLAG_LEVEL1)
                flags |= DECODE_FLAG_LEVEL2;
          else if (dflags & DECODE_FLAG_LEVEL2)
		flags |= DECODE_FLAG_LEVEL3;
	  else
		flags |= DECODE_FLAG_LEVEL1;

          result = _asn1_decode_simple_ber(etype, p, der_len, &out, &out_len, &tmp_len,
                                           flags);
          if (result != ASN1_SUCCESS)
            {
              warn();
              goto cleanup;
            }

          p += tmp_len;
          DECR_LEN(der_len, tmp_len);

          if (ber_len) *ber_len += tmp_len;

          DECR_LEN(der_len, 2); /* we need the EOC */

	  if (out_len > 0)
	    {
              result = append(&total, &total_size, out, out_len);
              if (result != ASN1_SUCCESS)
                {
                  warn();
                  goto cleanup;
                }
	    }

          free(out);
          out = NULL;

	  if (p[0] == 0 && p[1] == 0) /* EOC */
	    {
              if (ber_len) *ber_len += 2;
              break;
            }

          /* no EOC */
          der_len += 2;

          if (der_len == 2)
            {
              warn();
              result = ASN1_DER_ERROR;
              goto cleanup;
            }
        }
      while(1);
    }
  else if (class == ETYPE_CLASS(etype))
    {
      if (ber_len)
        {
          result = asn1_get_length_der (p, der_len, &len_len);
          if (result < 0)
            {
              warn();
              result = ASN1_DER_ERROR;
              goto cleanup;
            }
          *ber_len += result + len_len;
        }

      /* non-string values are decoded as DER */
      result = _asn1_decode_simple_der(etype, der, _der_len, &cout, &out_len, dflags);
      if (result != ASN1_SUCCESS)
        {
          warn();
          goto cleanup;
        }

      result = append(&total, &total_size, cout, out_len);
      if (result != ASN1_SUCCESS)
        {
          warn();
          goto cleanup;
        }
    }
  else
    {
      warn();
      result = ASN1_DER_ERROR;
      goto cleanup;
    }

  *str = total;
  *str_len = total_size;

  return ASN1_SUCCESS;
cleanup:
  free(out);
  free(total);
  return result;
}