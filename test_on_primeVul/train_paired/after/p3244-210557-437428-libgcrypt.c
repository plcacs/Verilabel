_gcry_mpi_ec_mul_point (mpi_point_t result,
                        gcry_mpi_t scalar, mpi_point_t point,
                        mpi_ec_t ctx)
{
  gcry_mpi_t x1, y1, z1, k, h, yy;
  unsigned int i, loops;
  mpi_point_struct p1, p2, p1inv;

  if (ctx->model == MPI_EC_EDWARDS
      || (ctx->model == MPI_EC_WEIERSTRASS
          && mpi_is_secure (scalar)))
    {
      /* Simple left to right binary method.  GECC Algorithm 3.27 */
      unsigned int nbits;
      int j;

      nbits = mpi_get_nbits (scalar);
      if (ctx->model == MPI_EC_WEIERSTRASS)
        {
          mpi_set_ui (result->x, 1);
          mpi_set_ui (result->y, 1);
          mpi_set_ui (result->z, 0);
        }
      else
        {
          mpi_set_ui (result->x, 0);
          mpi_set_ui (result->y, 1);
          mpi_set_ui (result->z, 1);
        }

      if (mpi_is_secure (scalar))
        {
          /* If SCALAR is in secure memory we assume that it is the
             secret key we use constant time operation.  */
          mpi_point_struct tmppnt;

          point_init (&tmppnt);
          point_resize (result, ctx);
          point_resize (&tmppnt, ctx);
          for (j=nbits-1; j >= 0; j--)
            {
              _gcry_mpi_ec_dup_point (result, result, ctx);
              _gcry_mpi_ec_add_points (&tmppnt, result, point, ctx);
              point_swap_cond (result, &tmppnt, mpi_test_bit (scalar, j), ctx);
            }
          point_free (&tmppnt);
        }
      else
        {
          for (j=nbits-1; j >= 0; j--)
            {
              _gcry_mpi_ec_dup_point (result, result, ctx);
              if (mpi_test_bit (scalar, j))
                _gcry_mpi_ec_add_points (result, result, point, ctx);
            }
        }
      return;
    }
  else if (ctx->model == MPI_EC_MONTGOMERY)
    {
      unsigned int nbits;
      int j;
      mpi_point_struct p1_, p2_;
      mpi_point_t q1, q2, prd, sum;
      unsigned long sw;

      /* Compute scalar point multiplication with Montgomery Ladder.
         Note that we don't use Y-coordinate in the points at all.
         RESULT->Y will be filled by zero.  */

      nbits = mpi_get_nbits (scalar);
      point_init (&p1);
      point_init (&p2);
      point_init (&p1_);
      point_init (&p2_);
      mpi_set_ui (p1.x, 1);
      mpi_free (p2.x);
      p2.x  = mpi_copy (point->x);
      mpi_set_ui (p2.z, 1);

      point_resize (&p1, ctx);
      point_resize (&p2, ctx);
      point_resize (&p1_, ctx);
      point_resize (&p2_, ctx);

      q1 = &p1;
      q2 = &p2;
      prd = &p1_;
      sum = &p2_;

      for (j=nbits-1; j >= 0; j--)
        {
          mpi_point_t t;

          sw = mpi_test_bit (scalar, j);
          point_swap_cond (q1, q2, sw, ctx);
          montgomery_ladder (prd, sum, q1, q2, point->x, ctx);
          point_swap_cond (prd, sum, sw, ctx);
          t = q1;  q1 = prd;  prd = t;
          t = q2;  q2 = sum;  sum = t;
        }

      mpi_clear (result->y);
      sw = (nbits & 1);
      point_swap_cond (&p1, &p1_, sw, ctx);

      if (p1.z->nlimbs == 0)
        {
          mpi_set_ui (result->x, 1);
          mpi_set_ui (result->z, 0);
        }
      else
        {
          z1 = mpi_new (0);
          ec_invm (z1, p1.z, ctx);
          ec_mulm (result->x, p1.x, z1, ctx);
          mpi_set_ui (result->z, 1);
          mpi_free (z1);
        }

      point_free (&p1);
      point_free (&p2);
      point_free (&p1_);
      point_free (&p2_);
      return;
    }

  x1 = mpi_alloc_like (ctx->p);
  y1 = mpi_alloc_like (ctx->p);
  h  = mpi_alloc_like (ctx->p);
  k  = mpi_copy (scalar);
  yy = mpi_copy (point->y);

  if ( mpi_has_sign (k) )
    {
      k->sign = 0;
      ec_invm (yy, yy, ctx);
    }

  if (!mpi_cmp_ui (point->z, 1))
    {
      mpi_set (x1, point->x);
      mpi_set (y1, yy);
    }
  else
    {
      gcry_mpi_t z2, z3;

      z2 = mpi_alloc_like (ctx->p);
      z3 = mpi_alloc_like (ctx->p);
      ec_mulm (z2, point->z, point->z, ctx);
      ec_mulm (z3, point->z, z2, ctx);
      ec_invm (z2, z2, ctx);
      ec_mulm (x1, point->x, z2, ctx);
      ec_invm (z3, z3, ctx);
      ec_mulm (y1, yy, z3, ctx);
      mpi_free (z2);
      mpi_free (z3);
    }
  z1 = mpi_copy (mpi_const (MPI_C_ONE));

  mpi_mul (h, k, mpi_const (MPI_C_THREE)); /* h = 3k */
  loops = mpi_get_nbits (h);
  if (loops < 2)
    {
      /* If SCALAR is zero, the above mpi_mul sets H to zero and thus
         LOOPs will be zero.  To avoid an underflow of I in the main
         loop we set LOOP to 2 and the result to (0,0,0).  */
      loops = 2;
      mpi_clear (result->x);
      mpi_clear (result->y);
      mpi_clear (result->z);
    }
  else
    {
      mpi_set (result->x, point->x);
      mpi_set (result->y, yy);
      mpi_set (result->z, point->z);
    }
  mpi_free (yy); yy = NULL;

  p1.x = x1; x1 = NULL;
  p1.y = y1; y1 = NULL;
  p1.z = z1; z1 = NULL;
  point_init (&p2);
  point_init (&p1inv);

  for (i=loops-2; i > 0; i--)
    {
      _gcry_mpi_ec_dup_point (result, result, ctx);
      if (mpi_test_bit (h, i) == 1 && mpi_test_bit (k, i) == 0)
        {
          point_set (&p2, result);
          _gcry_mpi_ec_add_points (result, &p2, &p1, ctx);
        }
      if (mpi_test_bit (h, i) == 0 && mpi_test_bit (k, i) == 1)
        {
          point_set (&p2, result);
          /* Invert point: y = p - y mod p  */
          point_set (&p1inv, &p1);
          ec_subm (p1inv.y, ctx->p, p1inv.y, ctx);
          _gcry_mpi_ec_add_points (result, &p2, &p1inv, ctx);
        }
    }

  point_free (&p1);
  point_free (&p2);
  point_free (&p1inv);
  mpi_free (h);
  mpi_free (k);
}