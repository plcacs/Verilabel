mix_pool(byte *pool)
{
    char *hashbuf = pool + POOLSIZE;
    char *p, *pend;
    int i, n;
    RMD160_CONTEXT md;

    rmd160_init( &md );
#if DIGESTLEN != 20
#error must have a digest length of 20 for ripe-md-160
#endif
    /* pool -> pool' */
    pend = pool + POOLSIZE;
    memcpy(hashbuf, pend - DIGESTLEN, DIGESTLEN );
    memcpy(hashbuf+DIGESTLEN, pool, BLOCKLEN-DIGESTLEN);
    rmd160_mixblock( &md, hashbuf);
    memcpy(pool, hashbuf, DIGESTLEN);

    /* Loop for the remaining iterations.  */
    p = pool;
    for( n=1; n < POOLBLOCKS; n++ ) {
	if( p + BLOCKLEN < pend )
	    memcpy(hashbuf, p, BLOCKLEN);
	else {
	    char *pp = p;
	    for(i=0; i < BLOCKLEN; i++ ) {
		if( pp >= pend )
		    pp = pool;
		hashbuf[i] = *pp++;
	    }
	}

	rmd160_mixblock( &md, hashbuf);
        p += DIGESTLEN;
	memcpy(p, hashbuf, DIGESTLEN);
    }
    burn_stack (384); /* for the rmd160_mixblock() */
}