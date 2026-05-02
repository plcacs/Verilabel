static void urlParsePostBody(struct URL *url,
                             const struct HttpConnection *http,
                             const char *buf, int len) {
  struct HashMap contentType;
  initHashMap(&contentType, urlDestroyHashMapEntry, NULL);
  const char *ctHeader     = getFromHashMap(&http->header, "content-type");
  urlParseHeaderLine(&contentType, ctHeader, ctHeader ? strlen(ctHeader) : 0);
  if (getRefFromHashMap(&contentType, "application/x-www-form-urlencoded")) {
    urlParseQueryString(&url->args, buf, len);
  } else if (getRefFromHashMap(&contentType, "multipart/form-data")) {
    const char *boundary   = getFromHashMap(&contentType, "boundary");
    if (boundary && *boundary) {
      const char *lastPart = NULL;
      for (const char *part = buf; len > 0; ) {
        const char *ptr;
        if ((part == buf && (ptr = urlMemstr(part, len, "--")) != NULL) ||
            (ptr = urlMemstr(part, len, "\r\n--")) != NULL) {
          len             -= ptr - part + (part == buf ? 2 : 4);
          part             = ptr + (part == buf ? 2 : 4);
          if (!urlMemcmp(part, len, boundary)) {
            int i          = strlen(boundary);
            len           -= i;
            part          += i;
            if (!urlMemcmp(part, len, "\r\n")) {
              len         -= 2;
              part        += 2;
              if (lastPart) {
                urlParsePart(url, lastPart, ptr - lastPart);
              } else {
                if (ptr != buf) {
                  info("[http] Ignoring prologue before \"multipart/form-data\"!");
                }
              }
              lastPart     = part;
            } else if (!urlMemcmp(part, len, "--\r\n")) {
              len         -= 4;
              part        += 4;
              urlParsePart(url, lastPart, ptr - lastPart);
              lastPart     = NULL;
              if (len > 0) {
                info("[http] Ignoring epilogue past end of \"multipart/"
				     "form-data\"!");
              }
            }
          }
        }
        /* elf-2018.09.09: Detection of broken multipart/form-data
           fixes DoS vulnerability.

           On 9/9/18 10:43 AM, Imre Rad wrote:
           Hi Markus, Marc!

           I identified a vulnerability today in Shellinabox, it is
           remote a denial of service, shellinaboxd eating up 100% cpu
           and not processing subsequent requests after the attack was
           mounted.
        */
        else {
          warn ("[http] Ignorning broken multipart/form-data");
          break;
        }
      }
      if (lastPart) {
        warn("[http] Missing final \"boundary\" for \"multipart/form-data\"!");
      }
    } else {
      warn("[http] Missing \"boundary\" information for \"multipart/form-data\"!");
    }
  }
  destroyHashMap(&contentType);
}