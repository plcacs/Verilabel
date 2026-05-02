static size_t send_control_msg(VirtIOSerial *vser, void *buf, size_t len)
{
    VirtQueueElement elem;
    VirtQueue *vq;

    vq = vser->c_ivq;
    if (!virtio_queue_ready(vq)) {
        return 0;
    }
    if (!virtqueue_pop(vq, &elem)) {
        return 0;
    }

    /* TODO: detect a buffer that's too short, set NEEDS_RESET */
    iov_from_buf(elem.in_sg, elem.in_num, 0, buf, len);

    virtqueue_push(vq, &elem, len);
    virtio_notify(VIRTIO_DEVICE(vser), vq);
    return len;
}