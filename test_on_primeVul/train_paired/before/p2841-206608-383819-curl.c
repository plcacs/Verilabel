static CURLcode imap_state_fetch_resp(struct connectdata *conn, int imapcode,
                                      imapstate instate)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  struct imap_conn *imapc = &conn->proto.imapc;
  struct pingpong *pp = &imapc->pp;
  const char *ptr = data->state.buffer;
  bool parsed = FALSE;
  curl_off_t size = 0;

  (void)instate; /* no use for this yet */

  if(imapcode != '*') {
    Curl_pgrsSetDownloadSize(data, -1);
    state(conn, IMAP_STOP);
    return CURLE_REMOTE_FILE_NOT_FOUND; /* TODO: Fix error code */
  }

  /* Something like this is received "* 1 FETCH (BODY[TEXT] {2021}\r" so parse
     the continuation data contained within the curly brackets */
  while(*ptr && (*ptr != '{'))
    ptr++;

  if(*ptr == '{') {
    char *endptr;
    if(!curlx_strtoofft(ptr + 1, &endptr, 10, &size)) {
      if(endptr - ptr > 1 && endptr[0] == '}' &&
         endptr[1] == '\r' && endptr[2] == '\0')
        parsed = TRUE;
    }
  }

  if(parsed) {
    infof(data, "Found %" CURL_FORMAT_CURL_OFF_TU " bytes to download\n",
          size);
    Curl_pgrsSetDownloadSize(data, size);

    if(pp->cache) {
      /* At this point there is a bunch of data in the header "cache" that is
         actually body content, send it as body and then skip it. Do note
         that there may even be additional "headers" after the body. */
      size_t chunk = pp->cache_size;

      if(chunk > (size_t)size)
        /* The conversion from curl_off_t to size_t is always fine here */
        chunk = (size_t)size;

      result = Curl_client_write(conn, CLIENTWRITE_BODY, pp->cache, chunk);
      if(result)
        return result;

      data->req.bytecount += chunk;

      infof(data, "Written %" CURL_FORMAT_CURL_OFF_TU
            " bytes, %" CURL_FORMAT_CURL_OFF_TU
            " bytes are left for transfer\n", (curl_off_t)chunk,
            size - chunk);

      /* Have we used the entire cache or just part of it?*/
      if(pp->cache_size > chunk) {
        /* Only part of it so shrink the cache to fit the trailing data */
        memmove(pp->cache, pp->cache + chunk, pp->cache_size - chunk);
        pp->cache_size -= chunk;
      }
      else {
        /* Free the cache */
        Curl_safefree(pp->cache);

        /* Reset the cache size */
        pp->cache_size = 0;
      }
    }

    if(data->req.bytecount == size)
      /* The entire data is already transferred! */
      Curl_setup_transfer(conn, -1, -1, FALSE, NULL, -1, NULL);
    else {
      /* IMAP download */
      data->req.maxdownload = size;
      Curl_setup_transfer(conn, FIRSTSOCKET, size, FALSE, NULL, -1, NULL);
    }
  }
  else {
    /* We don't know how to parse this line */
    failf(pp->conn->data, "Failed to parse FETCH response.");
    result = CURLE_WEIRD_SERVER_REPLY;
  }

  /* End of DO phase */
  state(conn, IMAP_STOP);

  return result;
}