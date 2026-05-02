header_cache_t *imap_hcache_open(struct ImapData *idata, const char *path)
{
  struct ImapMbox mx;
  struct Url url;
  char cachepath[PATH_MAX];
  char mbox[PATH_MAX];

  if (path)
    imap_cachepath(idata, path, mbox, sizeof(mbox));
  else
  {
    if (!idata->ctx || imap_parse_path(idata->ctx->path, &mx) < 0)
      return NULL;

    imap_cachepath(idata, mx.mbox, mbox, sizeof(mbox));
    FREE(&mx.mbox);
  }

  mutt_account_tourl(&idata->conn->account, &url);
  url.path = mbox;
  url_tostring(&url, cachepath, sizeof(cachepath), U_PATH);

  return mutt_hcache_open(HeaderCache, cachepath, imap_hcache_namer);
}