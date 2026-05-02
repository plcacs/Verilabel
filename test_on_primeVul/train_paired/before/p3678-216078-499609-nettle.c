equal_h (const struct ecc_modulo *p,
	 const mp_limb_t *x1, const mp_limb_t *z1,
	 const mp_limb_t *x2, const mp_limb_t *z2,
	 mp_limb_t *scratch)
{
#define t0 scratch
#define t1 (scratch + p->size)

  ecc_mod_mul (p, t0, x1, z2, t0);
  if (mpn_cmp (t0, p->m, p->size) >= 0)
    mpn_sub_n (t0, t0, p->m, p->size);

  ecc_mod_mul (p, t1, x2, z1, t1);
  if (mpn_cmp (t1, p->m, p->size) >= 0)
    mpn_sub_n (t1, t1, p->m, p->size);

  return mpn_cmp (t0, t1, p->size) == 0;

#undef t0
#undef t1
}