set_file_metadata (const char *origin_url, const char *referrer_url, FILE *fp)
{
  /* Save metadata about where the file came from (requested, final URLs) to
   * user POSIX Extended Attributes of retrieved file.
   *
   * For more details about the user namespace see
   * [http://freedesktop.org/wiki/CommonExtendedAttributes] and
   * [http://0pointer.de/lennart/projects/mod_mime_xattr/].
   */
  int retval = -1;

  if (!origin_url || !fp)
    return retval;

  retval = write_xattr_metadata ("user.xdg.origin.url", escnonprint_uri (origin_url), fp);
  if ((!retval) && referrer_url)
    retval = write_xattr_metadata ("user.xdg.referrer.url", escnonprint_uri (referrer_url), fp);

  return retval;
}