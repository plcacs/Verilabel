static void warnf(struct Configurable *config, const char *fmt, ...)
{
  if(!(config->conf & CONF_MUTE)) {
    va_list ap;
    int len;
    char *ptr;
    char print_buffer[256];

    va_start(ap, fmt);
    va_start(ap, fmt);
    len = vsnprintf(print_buffer, sizeof(print_buffer), fmt, ap);
    va_end(ap);

    ptr = print_buffer;
    while(len > 0) {
      fputs(WARN_PREFIX, config->errors);

      if(len > (int)WARN_TEXTWIDTH) {
        int cut = WARN_TEXTWIDTH-1;

        while(!ISSPACE(ptr[cut]) && cut) {
          cut--;
        }

        fwrite(ptr, cut + 1, 1, config->errors);
        fputs("\n", config->errors);
        ptr += cut+1; /* skip the space too */
        len -= cut;
      }
      else {
        fputs(ptr, config->errors);
        len = 0;
      }
    }
  }
}