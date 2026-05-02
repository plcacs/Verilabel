static char *getsistring(FILE *f, uint32_t ptr, uint32_t len) {
  char *name;
  uint32_t i;

  if (!len) return NULL;
  if (len>400) len=400;
  name = cli_malloc(len);
  if (!name) {
    cli_dbgmsg("SIS: OOM\n");
    return NULL;
  }
  fseek(f, ptr, SEEK_SET);
  if (fread(name, len, 1, f)!=1) {
    cli_dbgmsg("SIS: Unable to read string\n");
    free(name);
    return NULL;
  }
  for (i = 0 ; i < len; i+=2) name[i/2] = name[i];
  name[i/2]='\0';
  return name;
}