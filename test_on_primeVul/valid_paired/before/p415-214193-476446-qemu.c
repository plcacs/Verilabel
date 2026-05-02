static void dp8393x_do_transmit_packets(dp8393xState *s)
{
    NetClientState *nc = qemu_get_queue(s->nic);
    int width, size;
    int tx_len, len;
    uint16_t i;

    width = (s->regs[SONIC_DCR] & SONIC_DCR_DW) ? 2 : 1;

    while (1) {
        /* Read memory */
        size = sizeof(uint16_t) * 6 * width;
        s->regs[SONIC_TTDA] = s->regs[SONIC_CTDA];
        DPRINTF("Transmit packet at %08x\n", dp8393x_ttda(s));
        address_space_read(&s->as, dp8393x_ttda(s) + sizeof(uint16_t) * width,
                           MEMTXATTRS_UNSPECIFIED, s->data, size);
        tx_len = 0;

        /* Update registers */
        s->regs[SONIC_TCR] = dp8393x_get(s, width, 0) & 0xf000;
        s->regs[SONIC_TPS] = dp8393x_get(s, width, 1);
        s->regs[SONIC_TFC] = dp8393x_get(s, width, 2);
        s->regs[SONIC_TSA0] = dp8393x_get(s, width, 3);
        s->regs[SONIC_TSA1] = dp8393x_get(s, width, 4);
        s->regs[SONIC_TFS] = dp8393x_get(s, width, 5);

        /* Handle programmable interrupt */
        if (s->regs[SONIC_TCR] & SONIC_TCR_PINT) {
            s->regs[SONIC_ISR] |= SONIC_ISR_PINT;
        } else {
            s->regs[SONIC_ISR] &= ~SONIC_ISR_PINT;
        }

        for (i = 0; i < s->regs[SONIC_TFC]; ) {
            /* Append fragment */
            len = s->regs[SONIC_TFS];
            if (tx_len + len > sizeof(s->tx_buffer)) {
                len = sizeof(s->tx_buffer) - tx_len;
            }
            address_space_read(&s->as, dp8393x_tsa(s), MEMTXATTRS_UNSPECIFIED,
                               &s->tx_buffer[tx_len], len);
            tx_len += len;

            i++;
            if (i != s->regs[SONIC_TFC]) {
                /* Read next fragment details */
                size = sizeof(uint16_t) * 3 * width;
                address_space_read(&s->as,
                                   dp8393x_ttda(s)
                                   + sizeof(uint16_t) * width * (4 + 3 * i),
                                   MEMTXATTRS_UNSPECIFIED, s->data,
                                   size);
                s->regs[SONIC_TSA0] = dp8393x_get(s, width, 0);
                s->regs[SONIC_TSA1] = dp8393x_get(s, width, 1);
                s->regs[SONIC_TFS] = dp8393x_get(s, width, 2);
            }
        }

        /* Handle Ethernet checksum */
        if (!(s->regs[SONIC_TCR] & SONIC_TCR_CRCI)) {
            /* Don't append FCS there, to look like slirp packets
             * which don't have one */
        } else {
            /* Remove existing FCS */
            tx_len -= 4;
            if (tx_len < 0) {
                SONIC_ERROR("tx_len is %d\n", tx_len);
                break;
            }
        }

        if (s->regs[SONIC_RCR] & (SONIC_RCR_LB1 | SONIC_RCR_LB0)) {
            /* Loopback */
            s->regs[SONIC_TCR] |= SONIC_TCR_CRSL;
            if (nc->info->can_receive(nc)) {
                s->loopback_packet = 1;
                nc->info->receive(nc, s->tx_buffer, tx_len);
            }
        } else {
            /* Transmit packet */
            qemu_send_packet(nc, s->tx_buffer, tx_len);
        }
        s->regs[SONIC_TCR] |= SONIC_TCR_PTX;

        /* Write status */
        dp8393x_put(s, width, 0,
                    s->regs[SONIC_TCR] & 0x0fff); /* status */
        size = sizeof(uint16_t) * width;
        address_space_write(&s->as, dp8393x_ttda(s),
                            MEMTXATTRS_UNSPECIFIED, s->data, size);

        if (!(s->regs[SONIC_CR] & SONIC_CR_HTX)) {
            /* Read footer of packet */
            size = sizeof(uint16_t) * width;
            address_space_read(&s->as,
                               dp8393x_ttda(s)
                               + sizeof(uint16_t) * width
                                 * (4 + 3 * s->regs[SONIC_TFC]),
                               MEMTXATTRS_UNSPECIFIED, s->data,
                               size);
            s->regs[SONIC_CTDA] = dp8393x_get(s, width, 0);
            if (s->regs[SONIC_CTDA] & SONIC_DESC_EOL) {
                /* EOL detected */
                break;
            }
        }
    }

    /* Done */
    s->regs[SONIC_CR] &= ~SONIC_CR_TXP;
    s->regs[SONIC_ISR] |= SONIC_ISR_TXDN;
    dp8393x_update_irq(s);
}