ecc_ecdsa_sign (const struct ecc_curve *ecc,
		const mp_limb_t *zp,
		/* Random nonce, must be invertible mod ecc group
		   order. */
		const mp_limb_t *kp,
		size_t length, const uint8_t *digest,
		mp_limb_t *rp, mp_limb_t *sp,
		mp_limb_t *scratch)
{
#define P	    scratch
#define kinv	    scratch
#define hp	    (scratch  + ecc->p.size) /* NOTE: ecc->p.size + 1 limbs! */
#define tp	    (scratch + 2*ecc->p.size)
  /* Procedure, according to RFC 6090, "KT-I". q denotes the group
     order.

     1. k <-- uniformly random, 0 < k < q

     2. R <-- (r_x, r_y) = k g

     3. s1 <-- r_x mod q

     4. s2 <-- (h + z*s1)/k mod q.
  */

  ecc->mul_g (ecc, P, kp, P + 3*ecc->p.size);
  /* x coordinate only, modulo q */
  ecc->h_to_a (ecc, 2, rp, P, P + 3*ecc->p.size);

  /* Invert k, uses up to 7 * ecc->p.size including scratch (for secp384). */
  ecc->q.invert (&ecc->q, kinv, kp, tp);
  
  /* Process hash digest */
  ecc_hash (&ecc->q, hp, length, digest);

  ecc_mod_mul (&ecc->q, tp, zp, rp, tp);
  ecc_mod_add (&ecc->q, hp, hp, tp);
  ecc_mod_mul (&ecc->q, tp, hp, kinv, tp);

  mpn_copyi (sp, tp, ecc->p.size);
#undef P
#undef hp
#undef kinv
#undef tp
}