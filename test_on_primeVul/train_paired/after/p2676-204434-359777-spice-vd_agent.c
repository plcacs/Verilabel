static void do_client_disconnect(void)
{
    g_hash_table_remove_all(active_xfers);
    if (client_connected) {
        udscs_server_write_all(server, VDAGENTD_CLIENT_DISCONNECTED, 0, 0,
                               NULL, 0);
        client_connected = false;
    }
}