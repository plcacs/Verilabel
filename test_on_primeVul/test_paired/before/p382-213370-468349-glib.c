g_socket_client_connected_callback (GObject      *source,
				    GAsyncResult *result,
				    gpointer      user_data)
{
  ConnectionAttempt *attempt = user_data;
  GSocketClientAsyncConnectData *data = attempt->data;
  GSList *l;
  GError *error = NULL;
  GProxy *proxy;
  const gchar *protocol;

  /* data is NULL once the task is completed */
  if (data && g_task_return_error_if_cancelled (data->task))
    {
      g_object_unref (data->task);
      connection_attempt_unref (attempt);
      return;
    }

  if (attempt->timeout_source)
    {
      g_source_destroy (attempt->timeout_source);
      g_clear_pointer (&attempt->timeout_source, g_source_unref);
    }

  if (!g_socket_connection_connect_finish (G_SOCKET_CONNECTION (source),
					   result, &error))
    {
      if (!g_cancellable_is_cancelled (attempt->cancellable))
        {
          clarify_connect_error (error, data->connectable, attempt->address);
          set_last_error (data, error);
        }
      else
        g_clear_error (&error);

      if (data)
        {
          connection_attempt_remove (attempt);
          enumerator_next_async (data);
        }
      else
        connection_attempt_unref (attempt);

      return;
    }

  data->socket = g_steal_pointer (&attempt->socket);
  data->connection = g_steal_pointer (&attempt->connection);

  for (l = data->connection_attempts; l; l = g_slist_next (l))
    {
      ConnectionAttempt *attempt_entry = l->data;
      g_cancellable_cancel (attempt_entry->cancellable);
      attempt_entry->data = NULL;
      connection_attempt_unref (attempt_entry);
    }
  g_slist_free (data->connection_attempts);
  data->connection_attempts = NULL;
  connection_attempt_unref (attempt);

  g_socket_connection_set_cached_remote_address ((GSocketConnection*)data->connection, NULL);
  g_socket_client_emit_event (data->client, G_SOCKET_CLIENT_CONNECTED, data->connectable, data->connection);

  /* wrong, but backward compatible */
  g_socket_set_blocking (data->socket, TRUE);

  if (!data->proxy_addr)
    {
      g_socket_client_tls_handshake (data);
      return;
    }

  protocol = g_proxy_address_get_protocol (data->proxy_addr);

  /* The connection should not be anything other than TCP,
   * but let's put a safety guard in case
   */
  if (!G_IS_TCP_CONNECTION (data->connection))
    {
      g_critical ("Trying to proxy over non-TCP connection, this is "
          "most likely a bug in GLib IO library.");

      g_set_error_literal (&data->last_error,
          G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
          _("Proxying over a non-TCP connection is not supported."));

      enumerator_next_async (data);
    }
  else if (g_hash_table_contains (data->client->priv->app_proxies, protocol))
    {
      /* Simply complete the connection, we don't want to do TLS handshake
       * as the application proxy handling may need proxy handshake first */
      g_socket_client_async_connect_complete (data);
    }
  else if ((proxy = g_proxy_get_default_for_protocol (protocol)))
    {
      g_socket_client_emit_event (data->client, G_SOCKET_CLIENT_PROXY_NEGOTIATING, data->connectable, data->connection);
      g_proxy_connect_async (proxy,
                             data->connection,
                             data->proxy_addr,
                             g_task_get_cancellable (data->task),
                             g_socket_client_proxy_connect_callback,
                             data);
      g_object_unref (proxy);
    }
  else
    {
      g_clear_error (&data->last_error);

      g_set_error (&data->last_error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
          _("Proxy protocol “%s” is not supported."),
          protocol);

      enumerator_next_async (data);
    }
}