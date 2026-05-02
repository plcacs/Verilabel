static void do_tx_packet(lan9118_state *s)
{
    int n;
    uint32_t status;

    /* FIXME: Honor TX disable, and allow queueing of packets.  */
    if (s->phy_control & 0x4000)  {
        /* This assumes the receive routine doesn't touch the VLANClient.  */
        lan9118_receive(qemu_get_queue(s->nic), s->txp->data, s->txp->len);
    } else {
        qemu_send_packet(qemu_get_queue(s->nic), s->txp->data, s->txp->len);
    }
    s->txp->fifo_used = 0;

    if (s->tx_status_fifo_used == 512) {
        /* Status FIFO full */
        return;
    }
    /* Add entry to status FIFO.  */
    status = s->txp->cmd_b & 0xffff0000u;
    DPRINTF("Sent packet tag:%04x len %d\n", status >> 16, s->txp->len);
    n = (s->tx_status_fifo_head + s->tx_status_fifo_used) & 511;
    s->tx_status_fifo[n] = status;
    s->tx_status_fifo_used++;
    if (s->tx_status_fifo_used == 512) {
        s->int_sts |= TSFF_INT;
        /* TODO: Stop transmission.  */
    }
}