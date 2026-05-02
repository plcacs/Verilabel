static void msf2_dma_tx(MSF2EmacState *s)
{
    NetClientState *nc = qemu_get_queue(s->nic);
    hwaddr desc = s->regs[R_DMA_TX_DESC];
    uint8_t buf[MAX_PKT_SIZE];
    EmacDesc d;
    int size;
    uint8_t pktcnt;
    uint32_t status;

    if (!(s->regs[R_CFG1] & R_CFG1_TX_EN_MASK)) {
        return;
    }

    while (1) {
        emac_load_desc(s, &d, desc);
        if (d.pktsize & EMPTY_MASK) {
            break;
        }
        size = d.pktsize & PKT_SIZE;
        address_space_read(&s->dma_as, d.pktaddr, MEMTXATTRS_UNSPECIFIED,
                           buf, size);
        /*
         * This is very basic way to send packets. Ideally there should be
         * a FIFO and packets should be sent out from FIFO only when
         * R_CFG1 bit 0 is set.
         */
        if (s->regs[R_CFG1] & R_CFG1_LB_EN_MASK) {
            nc->info->receive(nc, buf, size);
        } else {
            qemu_send_packet(nc, buf, size);
        }
        d.pktsize |= EMPTY_MASK;
        emac_store_desc(s, &d, desc);
        /* update sent packets count */
        status = s->regs[R_DMA_TX_STATUS];
        pktcnt = FIELD_EX32(status, DMA_TX_STATUS, PKTCNT);
        pktcnt++;
        s->regs[R_DMA_TX_STATUS] = FIELD_DP32(status, DMA_TX_STATUS,
                                              PKTCNT, pktcnt);
        s->regs[R_DMA_TX_STATUS] |= R_DMA_TX_STATUS_PKT_SENT_MASK;
        desc = d.next;
    }
    s->regs[R_DMA_TX_STATUS] |= R_DMA_TX_STATUS_UNDERRUN_MASK;
    s->regs[R_DMA_TX_CTL] &= ~R_DMA_TX_CTL_EN_MASK;
}