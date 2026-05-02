resp_new (char *head)
{
  char *hdr;
  int count, size;

  struct response *resp = xnew0 (struct response);
  resp->data = head;

  if (*head == '\0')
    {
      /* Empty head means that we're dealing with a headerless
         (HTTP/0.9) response.  In that case, don't set HEADERS at
         all.  */
      return resp;
    }

  /* Split HEAD into header lines, so that resp_header_* functions
     don't need to do this over and over again.  */

  size = count = 0;
  hdr = head;
  while (1)
    {
      DO_REALLOC (resp->headers, size, count + 1, const char *);
      resp->headers[count++] = hdr;

      /* Break upon encountering an empty line. */
      if (!hdr[0] || (hdr[0] == '\r' && hdr[1] == '\n') || hdr[0] == '\n')
        break;

      /* Find the end of HDR, including continuations. */
      for (;;)
        {
          char *end = strchr (hdr, '\n');

          if (end)
            hdr = end + 1;
          else
            hdr += strlen (hdr);

          if (*hdr != ' ' && *hdr != '\t')
            break;

          // continuation, transform \r and \n into spaces
          *end = ' ';
          if (end > head && end[-1] == '\r')
            end[-1] = ' ';
        }
    }
  DO_REALLOC (resp->headers, size, count + 1, const char *);
  resp->headers[count] = NULL;

  return resp;
}