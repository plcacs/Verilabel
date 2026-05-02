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

    memcpy(elem.in_sg[0].iov_base, buf, len);

    virtqueue_push(vq, &elem, len);
    virtio_notify(VIRTIO_DEVICE(vser), vq);
    return len;
}