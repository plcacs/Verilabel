static gboolean udscs_server_accept_cb(GSocketService    *service,
                                       GSocketConnection *socket_conn,
                                       GObject           *source_object,
                                       gpointer           user_data)
{
    struct udscs_server *server = user_data;
    UdscsConnection *new_conn;

    new_conn = g_object_new(UDSCS_TYPE_CONNECTION, NULL);
    new_conn->debug = server->debug;
    new_conn->read_callback = server->read_callback;
    g_object_ref(socket_conn);
    vdagent_connection_setup(VDAGENT_CONNECTION(new_conn),
                             G_IO_STREAM(socket_conn),
                             FALSE,
                             sizeof(struct udscs_message_header),
                             server->error_cb);

    server->connections = g_list_prepend(server->connections, new_conn);

    if (server->debug)
        syslog(LOG_DEBUG, "new client accepted: %p", new_conn);

    if (server->connect_callback)
        server->connect_callback(new_conn);

    return TRUE;
}