e1000_send_packet(E1000State *s, const uint8_t *buf, int size)
{
    static const int PTCregs[6] = { PTC64, PTC127, PTC255, PTC511,
                                    PTC1023, PTC1522 };

    NetClientState *nc = qemu_get_queue(s->nic);
    if (s->phy_reg[PHY_CTRL] & MII_CR_LOOPBACK) {
        nc->info->receive(nc, buf, size);
    } else {
        qemu_send_packet(nc, buf, size);
    }
    inc_tx_bcast_or_mcast_count(s, buf);
    e1000x_increase_size_stats(s->mac_reg, PTCregs, size);
}