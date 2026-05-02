static int setcharset(unsigned char **p, unsigned char *charset)
{
  setcharset_state state = CURLFNM_SCHS_DEFAULT;
  unsigned char rangestart = 0;
  unsigned char lastchar   = 0;
  bool something_found = FALSE;
  unsigned char c;
  for(;;) {
    c = **p;
    switch(state) {
    case CURLFNM_SCHS_DEFAULT:
      if(ISALNUM(c)) { /* ASCII value */
        rangestart = c;
        charset[c] = 1;
        (*p)++;
        state = CURLFNM_SCHS_MAYRANGE;
        something_found = TRUE;
      }
      else if(c == ']') {
        if(something_found)
          return SETCHARSET_OK;
        something_found = TRUE;
        state = CURLFNM_SCHS_RIGHTBR;
        charset[c] = 1;
        (*p)++;
      }
      else if(c == '[') {
        char c2 = *((*p) + 1);
        if(c2 == ':') { /* there has to be a keyword */
          (*p) += 2;
          if(parsekeyword(p, charset)) {
            state = CURLFNM_SCHS_DEFAULT;
          }
          else
            return SETCHARSET_FAIL;
        }
        else {
          charset[c] = 1;
          (*p)++;
        }
        something_found = TRUE;
      }
      else if(c == '?' || c == '*') {
        something_found = TRUE;
        charset[c] = 1;
        (*p)++;
      }
      else if(c == '^' || c == '!') {
        if(!something_found) {
          if(charset[CURLFNM_NEGATE]) {
            charset[c] = 1;
            something_found = TRUE;
          }
          else
            charset[CURLFNM_NEGATE] = 1; /* negate charset */
        }
        else
          charset[c] = 1;
        (*p)++;
      }
      else if(c == '\\') {
        c = *(++(*p));
        if(ISPRINT((c))) {
          something_found = TRUE;
          state = CURLFNM_SCHS_MAYRANGE;
          charset[c] = 1;
          rangestart = c;
          (*p)++;
        }
        else
          return SETCHARSET_FAIL;
      }
      else if(c == '\0') {
        return SETCHARSET_FAIL;
      }
      else {
        charset[c] = 1;
        (*p)++;
        something_found = TRUE;
      }
      break;
    case CURLFNM_SCHS_MAYRANGE:
      if(c == '-') {
        charset[c] = 1;
        (*p)++;
        lastchar = '-';
        state = CURLFNM_SCHS_MAYRANGE2;
      }
      else if(c == '[') {
        state = CURLFNM_SCHS_DEFAULT;
      }
      else if(ISALNUM(c)) {
        charset[c] = 1;
        (*p)++;
      }
      else if(c == '\\') {
        c = *(++(*p));
        if(ISPRINT(c)) {
          charset[c] = 1;
          (*p)++;
        }
        else
          return SETCHARSET_FAIL;
      }
      else if(c == ']') {
        return SETCHARSET_OK;
      }
      else
        return SETCHARSET_FAIL;
      break;
    case CURLFNM_SCHS_MAYRANGE2:
      if(c == ']') {
        return SETCHARSET_OK;
      }
      else if(c == '\\') {
        c = *(++(*p));
        if(ISPRINT(c)) {
          charset[c] = 1;
          state = CURLFNM_SCHS_DEFAULT;
          (*p)++;
        }
        else
          return SETCHARSET_FAIL;
      }
      else if(c >= rangestart) {
        if((ISLOWER(c) && ISLOWER(rangestart)) ||
           (ISDIGIT(c) && ISDIGIT(rangestart)) ||
           (ISUPPER(c) && ISUPPER(rangestart))) {
          charset[lastchar] = 0;
          rangestart++;
          while(rangestart++ <= c)
            charset[rangestart-1] = 1;
          (*p)++;
          state = CURLFNM_SCHS_DEFAULT;
        }
        else
          return SETCHARSET_FAIL;
      }
      else
        return SETCHARSET_FAIL;
      break;
    case CURLFNM_SCHS_RIGHTBR:
      if(c == '[') {
        state = CURLFNM_SCHS_RIGHTBRLEFTBR;
        charset[c] = 1;
        (*p)++;
      }
      else if(c == ']') {
        return SETCHARSET_OK;
      }
      else if(c == '\0') {
        return SETCHARSET_FAIL;
      }
      else if(ISPRINT(c)) {
        charset[c] = 1;
        (*p)++;
        state = CURLFNM_SCHS_DEFAULT;
      }
      else
        /* used 'goto fail' instead of 'return SETCHARSET_FAIL' to avoid a
         * nonsense warning 'statement not reached' at end of the fnc when
         * compiling on Solaris */
        goto fail;
      break;
    case CURLFNM_SCHS_RIGHTBRLEFTBR:
      if(c == ']') {
        return SETCHARSET_OK;
      }
      else {
        state  = CURLFNM_SCHS_DEFAULT;
        charset[c] = 1;
        (*p)++;
      }
      break;
    }
  }
fail:
  return SETCHARSET_FAIL;
}