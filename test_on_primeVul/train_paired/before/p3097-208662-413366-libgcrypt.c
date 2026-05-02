mix_pool(unsigned char *pool)
{
  static unsigned char failsafe_digest[DIGESTLEN];
  static int failsafe_digest_valid;

  unsigned char *hashbuf = pool + POOLSIZE;
  unsigned char *p, *pend;
  int i, n;
  SHA1_CONTEXT md;
  unsigned int nburn;

#if DIGESTLEN != 20
#error must have a digest length of 20 for SHA-1
#endif

  gcry_assert (pool_is_locked);
  _gcry_sha1_mixblock_init (&md);

  /* Loop over the pool.  */
  pend = pool + POOLSIZE;
  memcpy (hashbuf, pend - DIGESTLEN, DIGESTLEN );
  memcpy (hashbuf+DIGESTLEN, pool, BLOCKLEN-DIGESTLEN);
  nburn = _gcry_sha1_mixblock (&md, hashbuf);
  memcpy (pool, hashbuf, DIGESTLEN);

  if (failsafe_digest_valid && pool == rndpool)
    {
      for (i=0; i < DIGESTLEN; i++)
        pool[i] ^= failsafe_digest[i];
    }

  p = pool;
  for (n=1; n < POOLBLOCKS; n++)
    {
      memcpy (hashbuf, p, DIGESTLEN);

      p += DIGESTLEN;
      if (p+DIGESTLEN+BLOCKLEN < pend)
        memcpy (hashbuf+DIGESTLEN, p+DIGESTLEN, BLOCKLEN-DIGESTLEN);
      else
        {
          unsigned char *pp = p + DIGESTLEN;

          for (i=DIGESTLEN; i < BLOCKLEN; i++ )
            {
              if ( pp >= pend )
                pp = pool;
              hashbuf[i] = *pp++;
	    }
	}

      _gcry_sha1_mixblock (&md, hashbuf);
      memcpy(p, hashbuf, DIGESTLEN);
    }

  /* Our hash implementation does only leave small parts (64 bytes)
     of the pool on the stack, so it is okay not to require secure
     memory here.  Before we use this pool, it will be copied to the
     help buffer anyway. */
  if ( pool == rndpool)
    {
      _gcry_sha1_hash_buffer (failsafe_digest, pool, POOLSIZE);
      failsafe_digest_valid = 1;
    }

  _gcry_burn_stack (nburn);
}