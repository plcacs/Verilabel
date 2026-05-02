int qemu_can_send_packet(NetClientState *sender)
{
    int vm_running = runstate_is_running();

    if (!vm_running) {
        return 0;
    }

    if (!sender->peer) {
        return 1;
    }

    return qemu_can_receive_packet(sender->peer);
}