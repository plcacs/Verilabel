static void vhost_vsock_common_send_transport_reset(VHostVSockCommon *vvc)
{
    VirtQueueElement *elem;
    VirtQueue *vq = vvc->event_vq;
    struct virtio_vsock_event event = {
        .id = cpu_to_le32(VIRTIO_VSOCK_EVENT_TRANSPORT_RESET),
    };

    elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
    if (!elem) {
        error_report("vhost-vsock missed transport reset event");
        return;
    }

    if (elem->out_num) {
        error_report("invalid vhost-vsock event virtqueue element with "
                     "out buffers");
        goto out;
    }

    if (iov_from_buf(elem->in_sg, elem->in_num, 0,
                     &event, sizeof(event)) != sizeof(event)) {
        error_report("vhost-vsock event virtqueue element is too short");
        goto out;
    }

    virtqueue_push(vq, elem, sizeof(event));
    virtio_notify(VIRTIO_DEVICE(vvc), vq);

out:
    g_free(elem);
}