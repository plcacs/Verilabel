header_cache_t* imap_hcache_open (IMAP_DATA* idata, const char* path)
{
  IMAP_MBOX mx;
  ciss_url_t url;
  char cachepath[LONG_STRING];
  char mbox[LONG_STRING];

  if (path)
    imap_cachepath (idata, path, mbox, sizeof (mbox));
  else
  {
    if (!idata->ctx || imap_parse_path (idata->ctx->path, &mx) < 0)
      return NULL;

    imap_cachepath (idata, mx.mbox, mbox, sizeof (mbox));
    FREE (&mx.mbox);
  }

  mutt_account_tourl (&idata->conn->account, &url);
  url.path = mbox;
  url_ciss_tostring (&url, cachepath, sizeof (cachepath), U_PATH);

  return mutt_hcache_open (HeaderCache, cachepath, imap_hcache_namer);
}