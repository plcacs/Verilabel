CURLcode Curl_http_done(struct connectdata *conn,
                        CURLcode status, bool premature)
{
  struct SessionHandle *data = conn->data;
  struct HTTP *http =data->req.protop;

  Curl_unencode_cleanup(conn);

#ifdef USE_SPNEGO
  if(data->state.proxyneg.state == GSS_AUTHSENT ||
     data->state.negotiate.state == GSS_AUTHSENT) {
    /* add forbid re-use if http-code != 401/407 as a WA only needed for
     * 401/407 that signal auth failure (empty) otherwise state will be RECV
     * with current code */
    if((data->req.httpcode != 401) && (data->req.httpcode != 407))
      connclose(conn, "Negotiate transfer completed");
    Curl_cleanup_negotiate(data);
  }
#endif

  /* set the proper values (possibly modified on POST) */
  conn->fread_func = data->set.fread_func; /* restore */
  conn->fread_in = data->set.in; /* restore */
  conn->seek_func = data->set.seek_func; /* restore */
  conn->seek_client = data->set.seek_client; /* restore */

  if(http == NULL)
    return CURLE_OK;

  if(http->send_buffer) {
    Curl_send_buffer *buff = http->send_buffer;

    free(buff->buffer);
    free(buff);
    http->send_buffer = NULL; /* clear the pointer */
  }

  if(HTTPREQ_POST_FORM == data->set.httpreq) {
    data->req.bytecount = http->readbytecount + http->writebytecount;

    Curl_formclean(&http->sendit); /* Now free that whole lot */
    if(http->form.fp) {
      /* a file being uploaded was left opened, close it! */
      fclose(http->form.fp);
      http->form.fp = NULL;
    }
  }
  else if(HTTPREQ_PUT == data->set.httpreq)
    data->req.bytecount = http->readbytecount + http->writebytecount;

  if(status)
    return status;

  if(!premature && /* this check is pointless when DONE is called before the
                      entire operation is complete */
     !conn->bits.retry &&
     !data->set.connect_only &&
     ((http->readbytecount +
       data->req.headerbytecount -
       data->req.deductheadercount)) <= 0) {
    /* If this connection isn't simply closed to be retried, AND nothing was
       read from the HTTP server (that counts), this can't be right so we
       return an error here */
    failf(data, "Empty reply from server");
    return CURLE_GOT_NOTHING;
  }

  return CURLE_OK;
}