set_file_metadata (const struct url *origin_url, const struct url *referrer_url, FILE *fp)
{
  /* Save metadata about where the file came from (requested, final URLs) to
   * user POSIX Extended Attributes of retrieved file.
   *
   * For more details about the user namespace see
   * [http://freedesktop.org/wiki/CommonExtendedAttributes] and
   * [http://0pointer.de/lennart/projects/mod_mime_xattr/].
   */
  int retval = -1;
  char *value;

  if (!origin_url || !fp)
    return retval;

  value = url_string (origin_url, URL_AUTH_HIDE);
  retval = write_xattr_metadata ("user.xdg.origin.url", escnonprint_uri (value), fp);
  xfree (value);

  if (!retval && referrer_url)
    {
	  struct url u;

	  memset(&u, 0, sizeof(u));
      u.scheme = referrer_url->scheme;
      u.host = referrer_url->host;
      u.port = referrer_url->port;

      value = url_string (&u, 0);
      retval = write_xattr_metadata ("user.xdg.referrer.url", escnonprint_uri (value), fp);
      xfree (value);
    }

  return retval;
}