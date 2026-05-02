equal_h (const struct ecc_modulo *p,
	 const mp_limb_t *x1, const mp_limb_t *z1,
	 const mp_limb_t *x2, const mp_limb_t *z2,
	 mp_limb_t *scratch)
{
#define t0 scratch
#define t1 (scratch + p->size)

  ecc_mod_mul_canonical (p, t0, x1, z2, t0);
  ecc_mod_mul_canonical (p, t1, x2, z1, t1);

  return mpn_cmp (t0, t1, p->size) == 0;

#undef t0
#undef t1
}