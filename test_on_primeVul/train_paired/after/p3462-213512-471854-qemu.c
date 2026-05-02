static void rtl8139_transfer_frame(RTL8139State *s, uint8_t *buf, int size,
    int do_interrupt, const uint8_t *dot1q_buf)
{
    struct iovec *iov = NULL;
    struct iovec vlan_iov[3];

    if (!size)
    {
        DPRINTF("+++ empty ethernet frame\n");
        return;
    }

    if (dot1q_buf && size >= ETH_ALEN * 2) {
        iov = (struct iovec[3]) {
            { .iov_base = buf, .iov_len = ETH_ALEN * 2 },
            { .iov_base = (void *) dot1q_buf, .iov_len = VLAN_HLEN },
            { .iov_base = buf + ETH_ALEN * 2,
                .iov_len = size - ETH_ALEN * 2 },
        };

        memcpy(vlan_iov, iov, sizeof(vlan_iov));
        iov = vlan_iov;
    }

    if (TxLoopBack == (s->TxConfig & TxLoopBack))
    {
        size_t buf2_size;
        uint8_t *buf2;

        if (iov) {
            buf2_size = iov_size(iov, 3);
            buf2 = g_malloc(buf2_size);
            iov_to_buf(iov, 3, 0, buf2, buf2_size);
            buf = buf2;
        }

        DPRINTF("+++ transmit loopback mode\n");
        qemu_receive_packet(qemu_get_queue(s->nic), buf, size);

        if (iov) {
            g_free(buf2);
        }
    }
    else
    {
        if (iov) {
            qemu_sendv_packet(qemu_get_queue(s->nic), iov, 3);
        } else {
            qemu_send_packet(qemu_get_queue(s->nic), buf, size);
        }
    }
}