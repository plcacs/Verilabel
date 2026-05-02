_eddsa_hash (const struct ecc_modulo *m,
	     mp_limb_t *rp, size_t digest_size, const uint8_t *digest)
{
  mp_size_t nlimbs = (8*digest_size + GMP_NUMB_BITS - 1) / GMP_NUMB_BITS;

  mpn_set_base256_le (rp, nlimbs, digest, digest_size);

  if (nlimbs > 2*m->size)
    {
      /* Special case for Ed448: reduce rp to 2*m->size limbs.
	 After decoding rp from a hash of size 2*rn:

	 rp = r2 || r1 || r0

	 where r0 and r1 have m->size limbs.  Reduce this to:

	 rp = r1' || r0

	 where r1' has m->size limbs.  */
      mp_limb_t hi = rp[2*m->size];
      assert (nlimbs == 2*m->size + 1);

      hi = mpn_addmul_1 (rp + m->size, m->B, m->size, hi);
      assert (hi <= 1);
      hi = mpn_cnd_add_n (hi, rp + m->size, rp + m->size, m->B, m->size);
      assert (hi == 0);
    }
  m->mod (m, rp, rp);
}