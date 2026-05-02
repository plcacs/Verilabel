static void sungem_send_packet(SunGEMState *s, const uint8_t *buf,
                               int size)
{
    NetClientState *nc = qemu_get_queue(s->nic);

    if (s->macregs[MAC_XIFCFG >> 2] & MAC_XIFCFG_LBCK) {
        nc->info->receive(nc, buf, size);
    } else {
        qemu_send_packet(nc, buf, size);
    }
}