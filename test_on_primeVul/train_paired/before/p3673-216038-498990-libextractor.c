EXTRACTOR_dvi_extract_method (struct EXTRACTOR_ExtractContext *ec)
{
  unsigned int klen;
  uint32_t pos;
  uint32_t opos;
  unsigned int len;
  unsigned int pageCount;
  char pages[16];
  void *buf;
  unsigned char *data;
  uint64_t size;
  uint64_t off;
  ssize_t iret;
  
  if (40 >= (iret = ec->read (ec->cls, &buf, 1024)))
    return;
  data = buf;
  if ( (data[0] != 247) ||
       (data[1] != 2) )
    return;                /* cannot be DVI or unsupported version */
  klen = data[14];
  size = ec->get_size (ec->cls);
  if (size > 16 * 1024 * 1024)
    return; /* too large */
  if (NULL == (data = malloc ((size_t) size)))
    return; /* out of memory */
  memcpy (data, buf, iret);
  off = iret;
  while (off < size)
    {
      if (0 >= (iret = ec->read (ec->cls, &buf, 16 * 1024)))
	{
	  free (data);
	  return;
	}
      memcpy (&data[off], buf, iret);
      off += iret;
    }
  pos = size - 1;
  while ( (223 == data[pos]) &&
	  (pos > 0) )
    pos--;
  if ( (2 != data[pos]) ||
       (pos < 40) )
    goto CLEANUP;
  pos--;
  pos -= 4;
  /* assert pos at 'post_post tag' */
  if (data[pos] != 249)
    goto CLEANUP;
  opos = pos;
  pos = getIntAt (&data[opos + 1]);
  if ( (pos + 25 > size) ||
       (pos + 25 < pos) )
    goto CLEANUP;
  /* assert pos at 'post' command */
  if (data[pos] != 248)
    goto CLEANUP;
  pageCount = 0;
  opos = pos;
  pos = getIntAt (&data[opos + 1]);
  while (1)
    {
      if (UINT32_MAX == pos)
        break;
      if ( (pos + 45 > size) ||
	   (pos + 45 < pos) )
	goto CLEANUP;
      if (data[pos] != 139)     /* expect 'bop' */
	goto CLEANUP;
      pageCount++;
      opos = pos;
      pos = getIntAt (&data[opos + 41]);
      if (UINT32_MAX == pos)
        break;
      if (pos >= opos)
	goto CLEANUP;           /* invalid! */
    }
  /* ok, now we believe it's a dvi... */
  snprintf (pages,
	    sizeof (pages),
	    "%u", 
	    pageCount);
  if (0 != ec->proc (ec->cls, 
		     "dvi",
		     EXTRACTOR_METATYPE_PAGE_COUNT,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     pages,
		     strlen (pages) + 1))
    goto CLEANUP;
  if (0 != ec->proc (ec->cls, 
		     "dvi",
		     EXTRACTOR_METATYPE_MIMETYPE,
		     EXTRACTOR_METAFORMAT_UTF8,
		     "text/plain",
		     "application/x-dvi",
		     strlen ("application/x-dvi") + 1))
    goto CLEANUP;
  {
    char comment[klen + 1];
    
    comment[klen] = '\0';
    memcpy (comment, &data[15], klen);
    if (0 != ec->proc (ec->cls, 
		       "dvi",
		       EXTRACTOR_METATYPE_COMMENT,
		       EXTRACTOR_METAFORMAT_C_STRING,
		       "text/plain",
		       comment,
		       klen + 1))
      goto CLEANUP;
  }
  /* try to find PDF/ps special */
  pos = opos;
  while ( (size >= 100) &&
	  (pos < size - 100) )
    {
      switch (data[pos])
        {
        case 139:              /* begin page 'bop', we typically have to skip that one to
                                   find the zzz's */
          pos += 45;            /* skip bop */
          break;
        case 239:              /* zzz1 */
          len = data[pos + 1];
          if (pos + 2 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 2, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 2;
          break;
        case 240:              /* zzz2 */
          len = getShortAt (&data[pos + 1]);
          if (pos + 3 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 3, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 3;
          break;
        case 241:              /* zzz3, who uses that? */
          len = (getShortAt (&data[pos + 1])) + 65536 * data[pos + 3];
          if (pos + 4 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 4, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 4;
          break;
        case 242:              /* zzz4, hurray! */
          len = getIntAt (&data[pos + 1]);
          if (pos + 1 + len < size)
            if (0 != parseZZZ ((const char *) data, pos + 5, len, ec->proc, ec->cls))
	      goto CLEANUP;
          pos += len + 5;
          break;
        default:               /* unsupported opcode, abort scan */
	  goto CLEANUP;
        }
    }
 CLEANUP:
  free (data);
}