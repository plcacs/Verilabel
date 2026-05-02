do_conversion (const char *tocode, const char *fromcode, char const *in_org, size_t inlen, char **out)
{
  iconv_t cd;
  /* sXXXav : hummm hard to guess... */
  size_t len, done, outlen;
  int invalid = 0, tooshort = 0;
  char *s, *in, *in_save;

  cd = iconv_open (tocode, fromcode);
  if (cd == (iconv_t)(-1))
    {
      logprintf (LOG_VERBOSE, _("Conversion from %s to %s isn't supported\n"),
                 quote (fromcode), quote (tocode));
      *out = NULL;
      return false;
    }

  /* iconv() has to work on an unescaped string */
  in_save = in = xstrndup (in_org, inlen);
  url_unescape_except_reserved (in);
  inlen = strlen(in);

  len = outlen = inlen * 2;
  *out = s = xmalloc (outlen + 1);
  done = 0;

  DEBUGP (("iconv %s -> %s\n", tocode, fromcode));

  for (;;)
    {
      DEBUGP (("iconv outlen=%d inlen=%d\n", outlen, inlen));
      if (iconv (cd, (ICONV_CONST char **) &in, &inlen, out, &outlen) != (size_t)(-1) &&
          iconv (cd, NULL, NULL, out, &outlen) != (size_t)(-1))
        {
          *out = s;
          *(s + len - outlen - done) = '\0';
          xfree(in_save);
          iconv_close(cd);
          IF_DEBUG
          {
            /* not not print out embedded passwords, in_org might be an URL */
            if (!strchr(in_org, '@') && !strchr(*out, '@'))
              debug_logprintf ("converted '%s' (%s) -> '%s' (%s)\n", in_org, fromcode, *out, tocode);
            else
              debug_logprintf ("logging suppressed, strings may contain password\n");
          }
          return true;
        }

      /* Incomplete or invalid multibyte sequence */
      if (errno == EINVAL || errno == EILSEQ)
        {
          if (!invalid)
            logprintf (LOG_VERBOSE,
                      _("Incomplete or invalid multibyte sequence encountered\n"));

          invalid++;
          **out = *in;
          in++;
          inlen--;
          (*out)++;
          outlen--;
        }
      else if (errno == E2BIG) /* Output buffer full */
        {
          logprintf (LOG_VERBOSE,
                    _("Reallocate output buffer len=%d outlen=%d inlen=%d\n"), len, outlen, inlen);
          tooshort++;
          done = len;
          len = done + inlen * 2;
          s = xrealloc (s, len + 1);
          *out = s + done - outlen;
          outlen += inlen * 2;
        }
      else /* Weird, we got an unspecified error */
        {
          logprintf (LOG_VERBOSE, _("Unhandled errno %d\n"), errno);
          break;
        }
    }

    xfree(in_save);
    iconv_close(cd);
    IF_DEBUG
    {
      /* not not print out embedded passwords, in_org might be an URL */
      if (!strchr(in_org, '@') && !strchr(*out, '@'))
        debug_logprintf ("converted '%s' (%s) -> '%s' (%s)\n", in_org, fromcode, *out, tocode);
      else
        debug_logprintf ("logging suppressed, strings may contain password\n");
    }
    return false;
}