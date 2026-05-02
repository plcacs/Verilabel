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
        goto err;
    }

    if (iov_from_buf(elem->in_sg, elem->in_num, 0,
                     &event, sizeof(event)) != sizeof(event)) {
        error_report("vhost-vsock event virtqueue element is too short");
        goto err;
    }

    virtqueue_push(vq, elem, sizeof(event));
    virtio_notify(VIRTIO_DEVICE(vvc), vq);

    g_free(elem);
    return;

err:
    virtqueue_detach_element(vq, elem, 0);
    g_free(elem);
}