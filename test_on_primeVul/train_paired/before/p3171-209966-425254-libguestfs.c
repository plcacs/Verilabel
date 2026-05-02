guestfs___first_line_of_file (guestfs_h *g, const char *filename)
{
  CLEANUP_FREE char **lines = NULL; /* sic: not CLEANUP_FREE_STRING_LIST */
  int64_t size;
  char *ret;

  /* Don't trust guestfs_head_n not to break with very large files.
   * Check the file size is something reasonable first.
   */
  size = guestfs_filesize (g, filename);
  if (size == -1)
    /* guestfs_filesize failed and has already set error in handle */
    return NULL;
  if (size > MAX_SMALL_FILE_SIZE) {
    error (g, _("size of %s is unreasonably large (%" PRIi64 " bytes)"),
           filename, size);
    return NULL;
  }

  lines = guestfs_head_n (g, 1, filename);
  if (lines == NULL)
    return NULL;
  if (lines[0] == NULL) {
    guestfs___free_string_list (lines);
    /* Empty file: Return an empty string as explained above. */
    return safe_strdup (g, "");
  }
  /* lines[1] should be NULL because of '1' argument above ... */

  ret = lines[0];               /* caller frees */

  return ret;
}