static void gem_transmit(CadenceGEMState *s)
{
    uint32_t desc[DESC_MAX_NUM_WORDS];
    hwaddr packet_desc_addr;
    uint8_t     *p;
    unsigned    total_bytes;
    int q = 0;

    /* Do nothing if transmit is not enabled. */
    if (!(s->regs[GEM_NWCTRL] & GEM_NWCTRL_TXENA)) {
        return;
    }

    DB_PRINT("\n");

    /* The packet we will hand off to QEMU.
     * Packets scattered across multiple descriptors are gathered to this
     * one contiguous buffer first.
     */
    p = s->tx_packet;
    total_bytes = 0;

    for (q = s->num_priority_queues - 1; q >= 0; q--) {
        /* read current descriptor */
        packet_desc_addr = gem_get_tx_desc_addr(s, q);

        DB_PRINT("read descriptor 0x%" HWADDR_PRIx "\n", packet_desc_addr);
        address_space_read(&s->dma_as, packet_desc_addr,
                           MEMTXATTRS_UNSPECIFIED, desc,
                           sizeof(uint32_t) * gem_get_desc_len(s, false));
        /* Handle all descriptors owned by hardware */
        while (tx_desc_get_used(desc) == 0) {

            /* Do nothing if transmit is not enabled. */
            if (!(s->regs[GEM_NWCTRL] & GEM_NWCTRL_TXENA)) {
                return;
            }
            print_gem_tx_desc(desc, q);

            /* The real hardware would eat this (and possibly crash).
             * For QEMU let's lend a helping hand.
             */
            if ((tx_desc_get_buffer(s, desc) == 0) ||
                (tx_desc_get_length(desc) == 0)) {
                DB_PRINT("Invalid TX descriptor @ 0x%" HWADDR_PRIx "\n",
                         packet_desc_addr);
                break;
            }

            if (tx_desc_get_length(desc) > gem_get_max_buf_len(s, true) -
                                               (p - s->tx_packet)) {
                qemu_log_mask(LOG_GUEST_ERROR, "TX descriptor @ 0x%" \
                         HWADDR_PRIx " too large: size 0x%x space 0x%zx\n",
                         packet_desc_addr, tx_desc_get_length(desc),
                         gem_get_max_buf_len(s, true) - (p - s->tx_packet));
                gem_set_isr(s, q, GEM_INT_AMBA_ERR);
                break;
            }

            /* Gather this fragment of the packet from "dma memory" to our
             * contig buffer.
             */
            address_space_read(&s->dma_as, tx_desc_get_buffer(s, desc),
                               MEMTXATTRS_UNSPECIFIED,
                               p, tx_desc_get_length(desc));
            p += tx_desc_get_length(desc);
            total_bytes += tx_desc_get_length(desc);

            /* Last descriptor for this packet; hand the whole thing off */
            if (tx_desc_get_last(desc)) {
                uint32_t desc_first[DESC_MAX_NUM_WORDS];
                hwaddr desc_addr = gem_get_tx_desc_addr(s, q);

                /* Modify the 1st descriptor of this packet to be owned by
                 * the processor.
                 */
                address_space_read(&s->dma_as, desc_addr,
                                   MEMTXATTRS_UNSPECIFIED, desc_first,
                                   sizeof(desc_first));
                tx_desc_set_used(desc_first);
                address_space_write(&s->dma_as, desc_addr,
                                    MEMTXATTRS_UNSPECIFIED, desc_first,
                                    sizeof(desc_first));
                /* Advance the hardware current descriptor past this packet */
                if (tx_desc_get_wrap(desc)) {
                    s->tx_desc_addr[q] = gem_get_tx_queue_base_addr(s, q);
                } else {
                    s->tx_desc_addr[q] = packet_desc_addr +
                                         4 * gem_get_desc_len(s, false);
                }
                DB_PRINT("TX descriptor next: 0x%08x\n", s->tx_desc_addr[q]);

                s->regs[GEM_TXSTATUS] |= GEM_TXSTATUS_TXCMPL;
                gem_set_isr(s, q, GEM_INT_TXCMPL);

                /* Handle interrupt consequences */
                gem_update_int_status(s);

                /* Is checksum offload enabled? */
                if (s->regs[GEM_DMACFG] & GEM_DMACFG_TXCSUM_OFFL) {
                    net_checksum_calculate(s->tx_packet, total_bytes, CSUM_ALL);
                }

                /* Update MAC statistics */
                gem_transmit_updatestats(s, s->tx_packet, total_bytes);

                /* Send the packet somewhere */
                if (s->phy_loop || (s->regs[GEM_NWCTRL] &
                                    GEM_NWCTRL_LOCALLOOP)) {
                    gem_receive(qemu_get_queue(s->nic), s->tx_packet,
                                total_bytes);
                } else {
                    qemu_send_packet(qemu_get_queue(s->nic), s->tx_packet,
                                     total_bytes);
                }

                /* Prepare for next packet */
                p = s->tx_packet;
                total_bytes = 0;
            }

            /* read next descriptor */
            if (tx_desc_get_wrap(desc)) {

                if (s->regs[GEM_DMACFG] & GEM_DMACFG_ADDR_64B) {
                    packet_desc_addr = s->regs[GEM_TBQPH];
                    packet_desc_addr <<= 32;
                } else {
                    packet_desc_addr = 0;
                }
                packet_desc_addr |= gem_get_tx_queue_base_addr(s, q);
            } else {
                packet_desc_addr += 4 * gem_get_desc_len(s, false);
            }
            DB_PRINT("read descriptor 0x%" HWADDR_PRIx "\n", packet_desc_addr);
            address_space_read(&s->dma_as, packet_desc_addr,
                               MEMTXATTRS_UNSPECIFIED, desc,
                               sizeof(uint32_t) * gem_get_desc_len(s, false));
        }

        if (tx_desc_get_used(desc)) {
            s->regs[GEM_TXSTATUS] |= GEM_TXSTATUS_USED;
            /* IRQ TXUSED is defined only for queue 0 */
            if (q == 0) {
                gem_set_isr(s, 0, GEM_INT_TXUSED);
            }
            gem_update_int_status(s);
        }
    }
}