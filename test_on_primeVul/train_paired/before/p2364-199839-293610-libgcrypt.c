secret_core_crt (gcry_mpi_t M, gcry_mpi_t C,
                 gcry_mpi_t D, unsigned int Nlimbs,
                 gcry_mpi_t P, gcry_mpi_t Q, gcry_mpi_t U)
{
  gcry_mpi_t m1 = mpi_alloc_secure ( Nlimbs + 1 );
  gcry_mpi_t m2 = mpi_alloc_secure ( Nlimbs + 1 );
  gcry_mpi_t h  = mpi_alloc_secure ( Nlimbs + 1 );

  /* m1 = c ^ (d mod (p-1)) mod p */
  mpi_sub_ui ( h, P, 1 );
  mpi_fdiv_r ( h, D, h );
  mpi_powm ( m1, C, h, P );

  /* m2 = c ^ (d mod (q-1)) mod q */
  mpi_sub_ui ( h, Q, 1  );
  mpi_fdiv_r ( h, D, h );
  mpi_powm ( m2, C, h, Q );

  /* h = u * ( m2 - m1 ) mod q */
  mpi_sub ( h, m2, m1 );
  if ( mpi_has_sign ( h ) )
    mpi_add ( h, h, Q );
  mpi_mulm ( h, U, h, Q );

  /* m = m1 + h * p */
  mpi_mul ( h, h, P );
  mpi_add ( M, m1, h );

  mpi_free ( h );
  mpi_free ( m1 );
  mpi_free ( m2 );
}