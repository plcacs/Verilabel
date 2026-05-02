static int bufp_alloc(USBRedirDevice *dev, uint8_t *data, uint16_t len,
    uint8_t status, uint8_t ep, void *free_on_destroy)
{
    struct buf_packet *bufp;

    if (!dev->endpoint[EP2I(ep)].bufpq_dropping_packets &&
        dev->endpoint[EP2I(ep)].bufpq_size >
            2 * dev->endpoint[EP2I(ep)].bufpq_target_size) {
        DPRINTF("bufpq overflow, dropping packets ep %02X\n", ep);
        dev->endpoint[EP2I(ep)].bufpq_dropping_packets = 1;
    }
    /* Since we're interupting the stream anyways, drop enough packets to get
       back to our target buffer size */
    if (dev->endpoint[EP2I(ep)].bufpq_dropping_packets) {
        if (dev->endpoint[EP2I(ep)].bufpq_size >
                dev->endpoint[EP2I(ep)].bufpq_target_size) {
            free(data);
            return -1;
        }
        dev->endpoint[EP2I(ep)].bufpq_dropping_packets = 0;
    }

    bufp = g_new(struct buf_packet, 1);
    bufp->data   = data;
    bufp->len    = len;
    bufp->offset = 0;
    bufp->status = status;
    bufp->free_on_destroy = free_on_destroy;
    QTAILQ_INSERT_TAIL(&dev->endpoint[EP2I(ep)].bufpq, bufp, next);
    dev->endpoint[EP2I(ep)].bufpq_size++;
    return 0;
}