static int usb_host_handle_control(USBHostDevice *s, USBPacket *p)
{
    struct usbdevfs_urb *urb;
    AsyncURB *aurb;
    int ret, value, index;

    /* 
     * Process certain standard device requests.
     * These are infrequent and are processed synchronously.
     */
    value = le16_to_cpu(s->ctrl.req.wValue);
    index = le16_to_cpu(s->ctrl.req.wIndex);

    dprintf("husb: ctrl type 0x%x req 0x%x val 0x%x index %u len %u\n",
        s->ctrl.req.bRequestType, s->ctrl.req.bRequest, value, index, 
        s->ctrl.len);

    if (s->ctrl.req.bRequestType == 0) {
        switch (s->ctrl.req.bRequest) {
        case USB_REQ_SET_ADDRESS:
            return usb_host_set_address(s, value);

        case USB_REQ_SET_CONFIGURATION:
            return usb_host_set_config(s, value & 0xff);
        }
    }

    if (s->ctrl.req.bRequestType == 1 &&
                  s->ctrl.req.bRequest == USB_REQ_SET_INTERFACE)
        return usb_host_set_interface(s, index, value);

    /* The rest are asynchronous */

    aurb = async_alloc();
    aurb->hdev   = s;
    aurb->packet = p;

    /* 
     * Setup ctrl transfer.
     *
     * s->ctrl is layed out such that data buffer immediately follows
     * 'req' struct which is exactly what usbdevfs expects.
     */ 
    urb = &aurb->urb;

    urb->type     = USBDEVFS_URB_TYPE_CONTROL;
    urb->endpoint = p->devep;

    urb->buffer        = &s->ctrl.req;
    urb->buffer_length = 8 + s->ctrl.len;

    urb->usercontext = s;

    ret = ioctl(s->fd, USBDEVFS_SUBMITURB, urb);

    dprintf("husb: submit ctrl. len %u aurb %p\n", urb->buffer_length, aurb);

    if (ret < 0) {
        dprintf("husb: submit failed. errno %d\n", errno);
        async_free(aurb);

        switch(errno) {
        case ETIMEDOUT:
            return USB_RET_NAK;
        case EPIPE:
        default:
            return USB_RET_STALL;
        }
    }

    usb_defer_packet(p, async_cancel, aurb);
    return USB_RET_ASYNC;
}