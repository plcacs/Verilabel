gostdsa_vko (const struct ecc_scalar *priv,
		const struct ecc_point *pub,
		size_t ukm_length, const uint8_t *ukm,
		uint8_t *out)
{
  const struct ecc_curve *ecc = priv->ecc;
  unsigned bsize = (ecc_bit_size (ecc) + 7) / 8;
  mp_size_t size = ecc->p.size;
  mp_size_t itch = 4*size + ecc->mul_itch;
  mp_limb_t *scratch;

  if (itch < 5*size + ecc->h_to_a_itch)
      itch = 5*size + ecc->h_to_a_itch;

  assert (pub->ecc == ecc);
  assert (priv->ecc == ecc);
  assert (ukm_length <= bsize);

  scratch = gmp_alloc_limbs (itch);

#define UKM scratch
#define TEMP (scratch + 3*size)
#define XYZ scratch
#define TEMP_Y (scratch + 4*size)

  mpn_set_base256_le (UKM, size, ukm, ukm_length);

  /* If ukm is 0, set it to 1, otherwise the result will be allways equal to 0,
   * no matter what private and public keys are. See RFC 4357 referencing GOST
   * R 34.10-2001 (RFC 5832) Section 6.1 step 2. */
  if (mpn_zero_p (UKM, size))
    UKM[0] = 1;

  ecc_mod_mul (&ecc->q, TEMP, priv->p, UKM, TEMP); /* TEMP = UKM * priv */
  ecc->mul (ecc, XYZ, TEMP, pub->p, scratch + 4*size); /* XYZ = UKM * priv * pub */
  ecc->h_to_a (ecc, 0, TEMP, XYZ, scratch + 5*size); /* TEMP = XYZ */
  mpn_get_base256_le (out, bsize, TEMP, size);
  mpn_get_base256_le (out+bsize, bsize, TEMP_Y, size);
  gmp_free_limbs (scratch, itch);
}