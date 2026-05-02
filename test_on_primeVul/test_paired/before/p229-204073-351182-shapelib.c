static char ** split(const char *arg, const char *delim) {
  char *copy = dupstr(arg);
  char **result = NULL;
  int i = 0;

  for (char *cptr = strtok(copy, delim); cptr; cptr = strtok(NULL, delim)) {
    char **tmp = realloc (result, sizeof *result * (i + 1));
    if (!tmp && result) {
      while (i > 0) {
	free(result[--i]);
      }
      free(result);
      free(copy);
      return NULL;
    }
    result = tmp;
    result[i++] = dupstr(cptr);
  }

  free(copy);

  if (i) {
    char **tmp = realloc(result, sizeof *result * (i + 1));
    if (!tmp) {
      while (i > 0) {
	free(result[--i]);
      }
      free(result);
      free(copy);
      return NULL;
    }
    result = tmp;
    result[i++] = NULL;
  }

  return result;
}