static void xgmac_enet_send(XgmacState *s)
{
    struct desc bd;
    int frame_size;
    int len;
    uint8_t frame[8192];
    uint8_t *ptr;

    ptr = frame;
    frame_size = 0;
    while (1) {
        xgmac_read_desc(s, &bd, 0);
        if ((bd.ctl_stat & 0x80000000) == 0) {
            /* Run out of descriptors to transmit.  */
            break;
        }
        len = (bd.buffer1_size & 0xfff) + (bd.buffer2_size & 0xfff);

        if ((bd.buffer1_size & 0xfff) > 2048) {
            DEBUGF_BRK("qemu:%s:ERROR...ERROR...ERROR... -- "
                        "xgmac buffer 1 len on send > 2048 (0x%x)\n",
                         __func__, bd.buffer1_size & 0xfff);
        }
        if ((bd.buffer2_size & 0xfff) != 0) {
            DEBUGF_BRK("qemu:%s:ERROR...ERROR...ERROR... -- "
                        "xgmac buffer 2 len on send != 0 (0x%x)\n",
                        __func__, bd.buffer2_size & 0xfff);
        }
        if (len >= sizeof(frame)) {
            DEBUGF_BRK("qemu:%s: buffer overflow %d read into %zu "
                        "buffer\n" , __func__, len, sizeof(frame));
            DEBUGF_BRK("qemu:%s: buffer1.size=%d; buffer2.size=%d\n",
                        __func__, bd.buffer1_size, bd.buffer2_size);
        }

        cpu_physical_memory_read(bd.buffer1_addr, ptr, len);
        ptr += len;
        frame_size += len;
        if (bd.ctl_stat & 0x20000000) {
            /* Last buffer in frame.  */
            qemu_send_packet(qemu_get_queue(s->nic), frame, len);
            ptr = frame;
            frame_size = 0;
            s->regs[DMA_STATUS] |= DMA_STATUS_TI | DMA_STATUS_NIS;
        }
        bd.ctl_stat &= ~0x80000000;
        /* Write back the modified descriptor.  */
        xgmac_write_desc(s, &bd, 0);
    }
}