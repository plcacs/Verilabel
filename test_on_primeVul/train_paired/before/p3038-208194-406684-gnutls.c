_gnutls_ecc_ansi_x963_export (gnutls_ecc_curve_t curve, bigint_t x, bigint_t y,
                              gnutls_datum_t * out)
{
  int numlen = gnutls_ecc_curve_get_size (curve);
  int byte_size, ret;
  size_t size;

  if (numlen == 0)
    return gnutls_assert_val (GNUTLS_E_INVALID_REQUEST);

  out->size = 1 + 2 * numlen;

  out->data = gnutls_malloc (out->size);
  if (out->data == NULL)
    return gnutls_assert_val (GNUTLS_E_MEMORY_ERROR);

  memset (out->data, 0, out->size);

  /* store byte 0x04 */
  out->data[0] = 0x04;

  /* pad and store x */
  byte_size = (_gnutls_mpi_get_nbits (x) + 7) / 8;
  size = out->size - (1 + (numlen - byte_size));
  ret = _gnutls_mpi_print (x, &out->data[1 + (numlen - byte_size)], &size);
  if (ret < 0)
    return gnutls_assert_val (ret);

  byte_size = (_gnutls_mpi_get_nbits (y) + 7) / 8;
  size = out->size - (1 + (numlen + numlen - byte_size));
  ret =
    _gnutls_mpi_print (y, &out->data[1 + numlen + numlen - byte_size], &size);
  if (ret < 0)
    return gnutls_assert_val (ret);

  /* pad and store y */
  return 0;
}