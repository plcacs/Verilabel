static uint32_t get_cmd(ESPState *s, uint32_t maxlen)
{
    uint8_t buf[ESP_CMDFIFO_SZ];
    uint32_t dmalen, n;
    int target;

    target = s->wregs[ESP_WBUSID] & BUSID_DID;
    if (s->dma) {
        dmalen = MIN(esp_get_tc(s), maxlen);
        if (dmalen == 0) {
            return 0;
        }
        if (s->dma_memory_read) {
            s->dma_memory_read(s->dma_opaque, buf, dmalen);
            fifo8_push_all(&s->cmdfifo, buf, dmalen);
        } else {
            if (esp_select(s) < 0) {
                fifo8_reset(&s->cmdfifo);
                return -1;
            }
            esp_raise_drq(s);
            fifo8_reset(&s->cmdfifo);
            return 0;
        }
    } else {
        dmalen = MIN(fifo8_num_used(&s->fifo), maxlen);
        if (dmalen == 0) {
            return 0;
        }
        n = esp_fifo_pop_buf(&s->fifo, buf, dmalen);
        if (n >= 3) {
            buf[0] = buf[2] >> 5;
        }
        fifo8_push_all(&s->cmdfifo, buf, n);
    }
    trace_esp_get_cmd(dmalen, target);

    if (esp_select(s) < 0) {
        fifo8_reset(&s->cmdfifo);
        return -1;
    }
    return dmalen;
}