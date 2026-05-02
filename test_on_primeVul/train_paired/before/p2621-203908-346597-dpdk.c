vhost_user_check_and_alloc_queue_pair(struct virtio_net *dev,
			struct vhu_msg_context *ctx)
{
	uint32_t vring_idx;

	switch (ctx->msg.request.master) {
	case VHOST_USER_SET_VRING_KICK:
	case VHOST_USER_SET_VRING_CALL:
	case VHOST_USER_SET_VRING_ERR:
		vring_idx = ctx->msg.payload.u64 & VHOST_USER_VRING_IDX_MASK;
		break;
	case VHOST_USER_SET_VRING_NUM:
	case VHOST_USER_SET_VRING_BASE:
	case VHOST_USER_GET_VRING_BASE:
	case VHOST_USER_SET_VRING_ENABLE:
		vring_idx = ctx->msg.payload.state.index;
		break;
	case VHOST_USER_SET_VRING_ADDR:
		vring_idx = ctx->msg.payload.addr.index;
		break;
	default:
		return 0;
	}

	if (vring_idx >= VHOST_MAX_VRING) {
		VHOST_LOG_CONFIG(ERR, "(%s) invalid vring index: %u\n", dev->ifname, vring_idx);
		return -1;
	}

	if (dev->virtqueue[vring_idx])
		return 0;

	return alloc_vring_queue(dev, vring_idx);
}