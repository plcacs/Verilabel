_gcry_mpi_powm (gcry_mpi_t res,
                gcry_mpi_t base, gcry_mpi_t expo, gcry_mpi_t mod)
{
  /* Pointer to the limbs of the arguments, their size and signs. */
  mpi_ptr_t  rp, ep, mp, bp;
  mpi_size_t esize, msize, bsize, rsize;
  int               msign, bsign, rsign;
  /* Flags telling the secure allocation status of the arguments.  */
  int        esec,  msec,  bsec;
  /* Size of the result including space for temporary values.  */
  mpi_size_t size;
  /* Helper.  */
  int mod_shift_cnt;
  int negative_result;
  mpi_ptr_t mp_marker = NULL;
  mpi_ptr_t bp_marker = NULL;
  mpi_ptr_t ep_marker = NULL;
  mpi_ptr_t xp_marker = NULL;
  unsigned int mp_nlimbs = 0;
  unsigned int bp_nlimbs = 0;
  unsigned int ep_nlimbs = 0;
  unsigned int xp_nlimbs = 0;
  mpi_ptr_t precomp[SIZE_PRECOMP]; /* Pre-computed array: BASE^1, ^3, ^5, ... */
  mpi_size_t precomp_size[SIZE_PRECOMP];
  mpi_size_t W;
  mpi_ptr_t base_u;
  mpi_size_t base_u_size;
  mpi_size_t max_u_size;

  esize = expo->nlimbs;
  msize = mod->nlimbs;
  size = 2 * msize;
  msign = mod->sign;

  ep = expo->d;
  MPN_NORMALIZE(ep, esize);

  if (esize * BITS_PER_MPI_LIMB > 512)
    W = 5;
  else if (esize * BITS_PER_MPI_LIMB > 256)
    W = 4;
  else if (esize * BITS_PER_MPI_LIMB > 128)
    W = 3;
  else if (esize * BITS_PER_MPI_LIMB > 64)
    W = 2;
  else
    W = 1;

  esec = mpi_is_secure(expo);
  msec = mpi_is_secure(mod);
  bsec = mpi_is_secure(base);

  rp = res->d;

  if (!msize)
    _gcry_divide_by_zero();

  if (!esize)
    {
      /* Exponent is zero, result is 1 mod MOD, i.e., 1 or 0 depending
         on if MOD equals 1.  */
      res->nlimbs = (msize == 1 && mod->d[0] == 1) ? 0 : 1;
      if (res->nlimbs)
        {
          RESIZE_IF_NEEDED (res, 1);
          rp = res->d;
          rp[0] = 1;
        }
      res->sign = 0;
      goto leave;
    }

  /* Normalize MOD (i.e. make its most significant bit set) as
     required by mpn_divrem.  This will make the intermediate values
     in the calculation slightly larger, but the correct result is
     obtained after a final reduction using the original MOD value. */
  mp_nlimbs = msec? msize:0;
  mp = mp_marker = mpi_alloc_limb_space(msize, msec);
  count_leading_zeros (mod_shift_cnt, mod->d[msize-1]);
  if (mod_shift_cnt)
    _gcry_mpih_lshift (mp, mod->d, msize, mod_shift_cnt);
  else
    MPN_COPY( mp, mod->d, msize );

  bsize = base->nlimbs;
  bsign = base->sign;
  if (bsize > msize)
    {
      /* The base is larger than the module.  Reduce it.

         Allocate (BSIZE + 1) with space for remainder and quotient.
         (The quotient is (bsize - msize + 1) limbs.)  */
      bp_nlimbs = bsec ? (bsize + 1):0;
      bp = bp_marker = mpi_alloc_limb_space( bsize + 1, bsec );
      MPN_COPY ( bp, base->d, bsize );
      /* We don't care about the quotient, store it above the
       * remainder, at BP + MSIZE.  */
      _gcry_mpih_divrem( bp + msize, 0, bp, bsize, mp, msize );
      bsize = msize;
      /* Canonicalize the base, since we are going to multiply with it
         quite a few times.  */
      MPN_NORMALIZE( bp, bsize );
    }
  else
    bp = base->d;

  if (!bsize)
    {
      res->nlimbs = 0;
      res->sign = 0;
      goto leave;
    }


  /* Make BASE, EXPO not overlap with RES.  We don't need to check MOD
     because that has already been copied to the MP var.  */
  if ( rp == bp )
    {
      /* RES and BASE are identical.  Allocate temp. space for BASE.  */
      gcry_assert (!bp_marker);
      bp_nlimbs = bsec? bsize:0;
      bp = bp_marker = mpi_alloc_limb_space( bsize, bsec );
      MPN_COPY(bp, rp, bsize);
    }
  if ( rp == ep )
    {
      /* RES and EXPO are identical.  Allocate temp. space for EXPO.  */
      ep_nlimbs = esec? esize:0;
      ep = ep_marker = mpi_alloc_limb_space( esize, esec );
      MPN_COPY(ep, rp, esize);
    }

  /* Copy base to the result.  */
  if (res->alloced < size)
    {
      mpi_resize (res, size);
      rp = res->d;
    }

  /* Main processing.  */
  {
    mpi_size_t i, j, k;
    mpi_ptr_t xp;
    mpi_size_t xsize;
    int c;
    mpi_limb_t e;
    mpi_limb_t carry_limb;
    struct karatsuba_ctx karactx;
    mpi_ptr_t tp;

    xp_nlimbs = msec? (2 * (msize + 1)):0;
    xp = xp_marker = mpi_alloc_limb_space( 2 * (msize + 1), msec );

    memset( &karactx, 0, sizeof karactx );
    negative_result = (ep[0] & 1) && bsign;

    /* Precompute PRECOMP[], BASE^(2 * i + 1), BASE^1, ^3, ^5, ... */
    if (W > 1)                  /* X := BASE^2 */
      mul_mod (xp, &xsize, bp, bsize, bp, bsize, mp, msize, &karactx);
    base_u = precomp[0] = mpi_alloc_limb_space (bsize, esec);
    base_u_size = max_u_size = precomp_size[0] = bsize;
    MPN_COPY (precomp[0], bp, bsize);
    for (i = 1; i < (1 << (W - 1)); i++)
      {                         /* PRECOMP[i] = BASE^(2 * i + 1) */
        if (xsize >= base_u_size)
          mul_mod (rp, &rsize, xp, xsize, base_u, base_u_size,
                   mp, msize, &karactx);
        else
          mul_mod (rp, &rsize, base_u, base_u_size, xp, xsize,
                   mp, msize, &karactx);
        base_u = precomp[i] = mpi_alloc_limb_space (rsize, esec);
        base_u_size = precomp_size[i] = rsize;
        if (max_u_size < base_u_size)
          max_u_size = base_u_size;
        MPN_COPY (precomp[i], rp, rsize);
      }

    base_u = mpi_alloc_limb_space (max_u_size, esec);
    MPN_ZERO (base_u, max_u_size);

    i = esize - 1;

    /* Main loop.

       Make the result be pointed to alternately by XP and RP.  This
       helps us avoid block copying, which would otherwise be
       necessary with the overlap restrictions of
       _gcry_mpih_divmod. With 50% probability the result after this
       loop will be in the area originally pointed by RP (==RES->d),
       and with 50% probability in the area originally pointed to by XP. */
    rsign = 0;
    if (W == 1)
      {
        rsize = bsize;
      }
    else
      {
        rsize = msize;
        MPN_ZERO (rp, rsize);
      }
    MPN_COPY ( rp, bp, bsize );

    e = ep[i];
    count_leading_zeros (c, e);
    e = (e << c) << 1;
    c = BITS_PER_MPI_LIMB - 1 - c;

    j = 0;

    for (;;)
      if (e == 0)
        {
          j += c;
          if ( --i < 0 )
            break;

          e = ep[i];
          c = BITS_PER_MPI_LIMB;
        }
      else
        {
          int c0;
          mpi_limb_t e0;

          count_leading_zeros (c0, e);
          e = (e << c0);
          c -= c0;
          j += c0;

          e0 = (e >> (BITS_PER_MPI_LIMB - W));
          if (c >= W)
            c0 = 0;
          else
            {
              if ( --i < 0 )
                {
                  e0 = (e >> (BITS_PER_MPI_LIMB - c));
                  j += c - W;
                  goto last_step;
                }
              else
                {
                  c0 = c;
                  e = ep[i];
                  c = BITS_PER_MPI_LIMB;
                  e0 |= (e >> (BITS_PER_MPI_LIMB - (W - c0)));
                }
            }

          e = e << (W - c0);
          c -= (W - c0);

        last_step:
          count_trailing_zeros (c0, e0);
          e0 = (e0 >> c0) >> 1;

          /*
           *  base_u <= precomp[e0]
           *  base_u_size <= precomp_size[e0]
           */
          base_u_size = 0;
          for (k = 0; k < (1<< (W - 1)); k++)
            {
              struct gcry_mpi w, u;
              w.alloced = w.nlimbs = precomp_size[k];
              u.alloced = u.nlimbs = precomp_size[k];
              w.sign = u.sign = 0;
              w.flags = u.flags = 0;
              w.d = base_u;
              u.d = precomp[k];

              mpi_set_cond (&w, &u, k == e0);
              base_u_size |= ( precomp_size[k] & ((mpi_size_t)0 - (k == e0)) );
            }

          for (j += W - c0; j >= 0; j--)
            {
              mul_mod (xp, &xsize, rp, rsize,
                       j == 0 ? base_u : rp, j == 0 ? base_u_size : rsize,
                       mp, msize, &karactx);
              tp = rp; rp = xp; xp = tp;
              rsize = xsize;
            }

          j = c0;
          if ( i < 0 )
            break;
        }

    while (j--)
      {
        mul_mod (xp, &xsize, rp, rsize, rp, rsize, mp, msize, &karactx);
        tp = rp; rp = xp; xp = tp;
        rsize = xsize;
      }

    /* We shifted MOD, the modulo reduction argument, left
       MOD_SHIFT_CNT steps.  Adjust the result by reducing it with the
       original MOD.

       Also make sure the result is put in RES->d (where it already
       might be, see above).  */
    if ( mod_shift_cnt )
      {
        carry_limb = _gcry_mpih_lshift( res->d, rp, rsize, mod_shift_cnt);
        rp = res->d;
        if ( carry_limb )
          {
            rp[rsize] = carry_limb;
            rsize++;
          }
      }
    else if (res->d != rp)
      {
        MPN_COPY (res->d, rp, rsize);
        rp = res->d;
      }

    if ( rsize >= msize )
      {
        _gcry_mpih_divrem(rp + msize, 0, rp, rsize, mp, msize);
        rsize = msize;
      }

    /* Remove any leading zero words from the result.  */
    if ( mod_shift_cnt )
      _gcry_mpih_rshift( rp, rp, rsize, mod_shift_cnt);
    MPN_NORMALIZE (rp, rsize);

    _gcry_mpih_release_karatsuba_ctx (&karactx );
    for (i = 0; i < (1 << (W - 1)); i++)
      _gcry_mpi_free_limb_space( precomp[i], esec ? precomp_size[i] : 0 );
    _gcry_mpi_free_limb_space (base_u, esec ? max_u_size : 0);
  }

  /* Fixup for negative results.  */
  if ( negative_result && rsize )
    {
      if ( mod_shift_cnt )
        _gcry_mpih_rshift( mp, mp, msize, mod_shift_cnt);
      _gcry_mpih_sub( rp, mp, msize, rp, rsize);
      rsize = msize;
      rsign = msign;
      MPN_NORMALIZE(rp, rsize);
    }
  gcry_assert (res->d == rp);
  res->nlimbs = rsize;
  res->sign = rsign;

 leave:
  if (mp_marker)
    _gcry_mpi_free_limb_space( mp_marker, mp_nlimbs );
  if (bp_marker)
    _gcry_mpi_free_limb_space( bp_marker, bp_nlimbs );
  if (ep_marker)
    _gcry_mpi_free_limb_space( ep_marker, ep_nlimbs );
  if (xp_marker)
    _gcry_mpi_free_limb_space( xp_marker, xp_nlimbs );
}