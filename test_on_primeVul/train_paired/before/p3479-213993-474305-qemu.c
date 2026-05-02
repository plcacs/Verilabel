int qemu_can_send_packet(NetClientState *sender)
{
    int vm_running = runstate_is_running();

    if (!vm_running) {
        return 0;
    }

    if (!sender->peer) {
        return 1;
    }

    if (sender->peer->receive_disabled) {
        return 0;
    } else if (sender->peer->info->can_receive &&
               !sender->peer->info->can_receive(sender->peer)) {
        return 0;
    }
    return 1;
}