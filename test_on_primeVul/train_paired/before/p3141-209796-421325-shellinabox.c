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