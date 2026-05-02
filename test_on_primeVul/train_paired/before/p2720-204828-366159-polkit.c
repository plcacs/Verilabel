polkit_system_bus_name_get_creds_sync (PolkitSystemBusName           *system_bus_name,
				       guint32                       *out_uid,
				       guint32                       *out_pid,
				       GCancellable                  *cancellable,
				       GError                       **error)
{
  gboolean ret = FALSE;
  AsyncGetBusNameCredsData data = { 0, };
  GDBusConnection *connection = NULL;
  GMainContext *tmp_context = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, cancellable, error);
  if (connection == NULL)
    goto out;

  data.error = error;

  tmp_context = g_main_context_new ();
  g_main_context_push_thread_default (tmp_context);

  /* Do two async calls as it's basically as fast as one sync call.
   */
  g_dbus_connection_call (connection,
			  "org.freedesktop.DBus",       /* name */
			  "/org/freedesktop/DBus",      /* object path */
			  "org.freedesktop.DBus",       /* interface name */
			  "GetConnectionUnixUser",      /* method */
			  g_variant_new ("(s)", system_bus_name->name),
			  G_VARIANT_TYPE ("(u)"),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  on_retrieved_unix_uid_pid,
			  &data);
  g_dbus_connection_call (connection,
			  "org.freedesktop.DBus",       /* name */
			  "/org/freedesktop/DBus",      /* object path */
			  "org.freedesktop.DBus",       /* interface name */
			  "GetConnectionUnixProcessID", /* method */
			  g_variant_new ("(s)", system_bus_name->name),
			  G_VARIANT_TYPE ("(u)"),
			  G_DBUS_CALL_FLAGS_NONE,
			  -1,
			  cancellable,
			  on_retrieved_unix_uid_pid,
			  &data);

  while (!((data.retrieved_uid && data.retrieved_pid) || data.caught_error))
    g_main_context_iteration (tmp_context, TRUE);

  if (out_uid)
    *out_uid = data.uid;
  if (out_pid)
    *out_pid = data.pid;
  ret = TRUE;
 out:
  if (tmp_context)
    {
      g_main_context_pop_thread_default (tmp_context);
      g_main_context_unref (tmp_context);
    }
  if (connection != NULL)
    g_object_unref (connection);
  return ret;
}