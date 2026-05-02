static void do_client_disconnect(void)
{
    if (client_connected) {
        udscs_server_write_all(server, VDAGENTD_CLIENT_DISCONNECTED, 0, 0,
                               NULL, 0);
        client_connected = false;
    }
}