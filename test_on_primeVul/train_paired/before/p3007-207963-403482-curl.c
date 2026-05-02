static char *sanitize_cookie_path(const char *cookie_path)
{
  size_t len;
  char *new_path = strdup(cookie_path);
  if(!new_path)
    return NULL;

  /* some stupid site sends path attribute with '"'. */
  if(new_path[0] == '\"') {
    memmove((void *)new_path, (const void *)(new_path + 1), strlen(new_path));
  }
  if(new_path[strlen(new_path) - 1] == '\"') {
    new_path[strlen(new_path) - 1] = 0x0;
  }

  /* RFC6265 5.2.4 The Path Attribute */
  if(new_path[0] != '/') {
    /* Let cookie-path be the default-path. */
    free(new_path);
    new_path = strdup("/");
    return new_path;
  }

  /* convert /hoge/ to /hoge */
  len = strlen(new_path);
  if(1 < len && new_path[len - 1] == '/') {
    new_path[len - 1] = 0x0;
  }

  return new_path;
}