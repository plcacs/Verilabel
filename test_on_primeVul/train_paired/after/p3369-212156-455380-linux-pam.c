static int _expand_arg(pam_handle_t *pamh, char **value)
{
  const char *orig=*value, *tmpptr=NULL;
  char *ptr;       /*
		    * Sure would be nice to use tmpptr but it needs to be
		    * a constant so that the compiler will shut up when I
		    * call pam_getenv and _pam_get_item_byname -- sigh
		    */

  /* No unexpanded variable can be bigger than BUF_SIZE */
  char type, tmpval[BUF_SIZE];

  /* I know this shouldn't be hard-coded but it's so much easier this way */
  char tmp[MAX_ENV];

  D(("Remember to initialize tmp!"));
  memset(tmp, 0, MAX_ENV);

  /*
   * (possibly non-existent) environment variables can be used as values
   * by prepending a "$" and wrapping in {} (ie: ${HOST}), can escape with "\"
   * (possibly non-existent) PAM items can be used as values
   * by prepending a "@" and wrapping in {} (ie: @{PAM_RHOST}, can escape
   *
   */
  D(("Expanding <%s>",orig));
  while (*orig) {     /* while there is some input to deal with */
    if ('\\' == *orig) {
      ++orig;
      if ('$' != *orig && '@' != *orig) {
	D(("Unrecognized escaped character: <%c> - ignoring", *orig));
	pam_syslog(pamh, LOG_ERR,
		   "Unrecognized escaped character: <%c> - ignoring",
		   *orig);
      } else if ((strlen(tmp) + 1) < MAX_ENV) {
	tmp[strlen(tmp)] = *orig++;        /* Note the increment */
      } else {
	/* is it really a good idea to try to log this? */
	D(("Variable buffer overflow: <%s> + <%s>", tmp, tmpptr));
	pam_syslog (pamh, LOG_ERR, "Variable buffer overflow: <%s> + <%s>",
		 tmp, tmpptr);
	return PAM_BUF_ERR;
      }
      continue;
    }
    if ('$' == *orig || '@' == *orig) {
      if ('{' != *(orig+1)) {
	D(("Expandable variables must be wrapped in {}"
	   " <%s> - ignoring", orig));
	pam_syslog(pamh, LOG_ERR, "Expandable variables must be wrapped in {}"
		 " <%s> - ignoring", orig);
	if ((strlen(tmp) + 1) < MAX_ENV) {
	  tmp[strlen(tmp)] = *orig++;        /* Note the increment */
	}
	continue;
      } else {
	D(("Expandable argument: <%s>", orig));
	type = *orig;
	orig+=2;     /* skip the ${ or @{ characters */
	ptr = strchr(orig, '}');
	if (ptr) {
	  *ptr++ = '\0';
	} else {
	  D(("Unterminated expandable variable: <%s>", orig-2));
	  pam_syslog(pamh, LOG_ERR,
		     "Unterminated expandable variable: <%s>", orig-2);
	  return PAM_ABORT;
	}
	strncpy(tmpval, orig, sizeof(tmpval));
	tmpval[sizeof(tmpval)-1] = '\0';
	orig=ptr;
	/*
	 * so, we know we need to expand tmpval, it is either
	 * an environment variable or a PAM_ITEM. type will tell us which
	 */
	switch (type) {

	case '$':
	  D(("Expanding env var: <%s>",tmpval));
	  tmpptr = pam_getenv(pamh, tmpval);
	  D(("Expanded to <%s>", tmpptr));
	  break;

	case '@':
	  D(("Expanding pam item: <%s>",tmpval));
	  tmpptr = _pam_get_item_byname(pamh, tmpval);
	  D(("Expanded to <%s>", tmpptr));
	  break;

	default:
	  D(("Impossible error, type == <%c>", type));
	  pam_syslog(pamh, LOG_CRIT, "Impossible error, type == <%c>", type);
	  return PAM_ABORT;
	}         /* switch */

	if (tmpptr) {
	  if ((strlen(tmp) + strlen(tmpptr)) < MAX_ENV) {
	    strcat(tmp, tmpptr);
	  } else {
	    /* is it really a good idea to try to log this? */
	    D(("Variable buffer overflow: <%s> + <%s>", tmp, tmpptr));
	    pam_syslog (pamh, LOG_ERR,
			"Variable buffer overflow: <%s> + <%s>", tmp, tmpptr);
	    return PAM_BUF_ERR;
	  }
	}
      }           /* if ('{' != *orig++) */
    } else {      /* if ( '$' == *orig || '@' == *orig) */
      if ((strlen(tmp) + 1) < MAX_ENV) {
	tmp[strlen(tmp)] = *orig++;        /* Note the increment */
      } else {
	/* is it really a good idea to try to log this? */
	D(("Variable buffer overflow: <%s> + <%s>", tmp, tmpptr));
	pam_syslog(pamh, LOG_ERR,
		   "Variable buffer overflow: <%s> + <%s>", tmp, tmpptr);
	return PAM_BUF_ERR;
      }
    }
  }              /* for (;*orig;) */

  if (strlen(tmp) > strlen(*value)) {
    free(*value);
    if ((*value = malloc(strlen(tmp) +1)) == NULL) {
      D(("Couldn't malloc %d bytes for expanded var", strlen(tmp)+1));
      pam_syslog (pamh, LOG_ERR, "Couldn't malloc %lu bytes for expanded var",
	       (unsigned long)strlen(tmp)+1);
      return PAM_BUF_ERR;
    }
  }
  strcpy(*value, tmp);
  memset(tmp,'\0',sizeof(tmp));
  D(("Exit."));

  return PAM_SUCCESS;
}