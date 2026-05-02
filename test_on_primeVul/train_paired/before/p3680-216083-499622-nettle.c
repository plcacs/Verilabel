ecc_gostdsa_verify (const struct ecc_curve *ecc,
		  const mp_limb_t *pp, /* Public key */
		  size_t length, const uint8_t *digest,
		  const mp_limb_t *rp, const mp_limb_t *sp,
		  mp_limb_t *scratch)
{
  /* Procedure, according to GOST R 34.10. q denotes the group
     order.

     1. Check 0 < r, s < q.

     2. v <-- h^{-1}  (mod q)

     3. z1  <-- s * v (mod q)

     4. z2  <-- -r * v (mod q)

     5. R = u1 G + u2 Y

     6. Signature is valid if R_x = r (mod q).
  */

#define hp (scratch)
#define vp (scratch + ecc->p.size)
#define z1 (scratch + 3*ecc->p.size)
#define z2 (scratch + 4*ecc->p.size)

#define P1 (scratch + 4*ecc->p.size)
#define P2 (scratch)


  if (! (ecdsa_in_range (ecc, rp)
	 && ecdsa_in_range (ecc, sp)))
    return 0;

  gost_hash (&ecc->q, hp, length, digest);

  if (mpn_zero_p (hp, ecc->p.size))
    mpn_add_1 (hp, hp, ecc->p.size, 1);

  /* Compute v */
  ecc->q.invert (&ecc->q, vp, hp, vp + ecc->p.size);

  /* z1 = s / h, P1 = z1 * G */
  ecc_mod_mul (&ecc->q, z1, sp, vp, z1);

  /* z2 = - r / h, P2 = z2 * Y */
  ecc_mod_mul (&ecc->q, z2, rp, vp, z2);
  mpn_sub_n (z2, ecc->q.m, z2, ecc->p.size);

   /* Total storage: 5*ecc->p.size + ecc->mul_itch */
  ecc->mul (ecc, P2, z2, pp, z2 + ecc->p.size);

  /* Total storage: 7*ecc->p.size + ecc->mul_g_itch (ecc->p.size) */
  ecc->mul_g (ecc, P1, z1, P1 + 3*ecc->p.size);

  /* Total storage: 6*ecc->p.size + ecc->add_hhh_itch */
  ecc->add_hhh (ecc, P1, P1, P2, P1 + 3*ecc->p.size);

  /* x coordinate only, modulo q */
  ecc->h_to_a (ecc, 2, P2, P1, P1 + 3*ecc->p.size);

  return (mpn_cmp (rp, P2, ecc->p.size) == 0);
#undef P2
#undef P1
#undef z2
#undef z1
#undef hp
#undef vp
}