static void close_all_connections(struct Curl_multi *multi)
{
  struct connectdata *conn;

  conn = Curl_conncache_find_first_connection(&multi->conn_cache);
  while(conn) {
    SIGPIPE_VARIABLE(pipe_st);
    conn->data = multi->closure_handle;

    sigpipe_ignore(conn->data, &pipe_st);
    /* This will remove the connection from the cache */
    (void)Curl_disconnect(conn, FALSE);
    sigpipe_restore(&pipe_st);

    conn = Curl_conncache_find_first_connection(&multi->conn_cache);
  }
}