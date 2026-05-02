_gnutls_x509_oid2mac_algorithm (const char *oid)
{
  gnutls_mac_algorithm_t ret = 0;

  GNUTLS_HASH_LOOP (if (strcmp (oid, p->oid) == 0)
		    {
		    ret = p->id; break;}
  );

  if (ret == 0)
    return GNUTLS_MAC_UNKNOWN;
  return ret;
}