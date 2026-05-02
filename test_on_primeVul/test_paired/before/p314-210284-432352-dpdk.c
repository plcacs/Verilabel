vhost_user_get_inflight_fd(struct virtio_net **pdev,
			   struct vhu_msg_context *ctx,
			   int main_fd __rte_unused)
{
	struct rte_vhost_inflight_info_packed *inflight_packed;
	uint64_t pervq_inflight_size, mmap_size;
	uint16_t num_queues, queue_size;
	struct virtio_net *dev = *pdev;
	int fd, i, j;
	int numa_node = SOCKET_ID_ANY;
	void *addr;

	if (ctx->msg.size != sizeof(ctx->msg.payload.inflight)) {
		VHOST_LOG_CONFIG(ERR, "(%s) invalid get_inflight_fd message size is %d\n",
			dev->ifname, ctx->msg.size);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	/*
	 * If VQ 0 has already been allocated, try to allocate on the same
	 * NUMA node. It can be reallocated later in numa_realloc().
	 */
	if (dev->nr_vring > 0)
		numa_node = dev->virtqueue[0]->numa_node;

	if (dev->inflight_info == NULL) {
		dev->inflight_info = rte_zmalloc_socket("inflight_info",
				sizeof(struct inflight_mem_info), 0, numa_node);
		if (!dev->inflight_info) {
			VHOST_LOG_CONFIG(ERR, "(%s) failed to alloc dev inflight area\n",
					dev->ifname);
			return RTE_VHOST_MSG_RESULT_ERR;
		}
		dev->inflight_info->fd = -1;
	}

	num_queues = ctx->msg.payload.inflight.num_queues;
	queue_size = ctx->msg.payload.inflight.queue_size;

	VHOST_LOG_CONFIG(INFO, "(%s) get_inflight_fd num_queues: %u\n",
		dev->ifname, ctx->msg.payload.inflight.num_queues);
	VHOST_LOG_CONFIG(INFO, "(%s) get_inflight_fd queue_size: %u\n",
		dev->ifname, ctx->msg.payload.inflight.queue_size);

	if (vq_is_packed(dev))
		pervq_inflight_size = get_pervq_shm_size_packed(queue_size);
	else
		pervq_inflight_size = get_pervq_shm_size_split(queue_size);

	mmap_size = num_queues * pervq_inflight_size;
	addr = inflight_mem_alloc(dev, "vhost-inflight", mmap_size, &fd);
	if (!addr) {
		VHOST_LOG_CONFIG(ERR, "(%s) failed to alloc vhost inflight area\n", dev->ifname);
			ctx->msg.payload.inflight.mmap_size = 0;
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	memset(addr, 0, mmap_size);

	if (dev->inflight_info->addr) {
		munmap(dev->inflight_info->addr, dev->inflight_info->size);
		dev->inflight_info->addr = NULL;
	}

	if (dev->inflight_info->fd >= 0) {
		close(dev->inflight_info->fd);
		dev->inflight_info->fd = -1;
	}

	dev->inflight_info->addr = addr;
	dev->inflight_info->size = ctx->msg.payload.inflight.mmap_size = mmap_size;
	dev->inflight_info->fd = ctx->fds[0] = fd;
	ctx->msg.payload.inflight.mmap_offset = 0;
	ctx->fd_num = 1;

	if (vq_is_packed(dev)) {
		for (i = 0; i < num_queues; i++) {
			inflight_packed =
				(struct rte_vhost_inflight_info_packed *)addr;
			inflight_packed->used_wrap_counter = 1;
			inflight_packed->old_used_wrap_counter = 1;
			for (j = 0; j < queue_size; j++)
				inflight_packed->desc[j].next = j + 1;
			addr = (void *)((char *)addr + pervq_inflight_size);
		}
	}

	VHOST_LOG_CONFIG(INFO, "(%s) send inflight mmap_size: %"PRIu64"\n",
			dev->ifname, ctx->msg.payload.inflight.mmap_size);
	VHOST_LOG_CONFIG(INFO, "(%s) send inflight mmap_offset: %"PRIu64"\n",
			dev->ifname, ctx->msg.payload.inflight.mmap_offset);
	VHOST_LOG_CONFIG(INFO, "(%s) send inflight fd: %d\n", dev->ifname, ctx->fds[0]);

	return RTE_VHOST_MSG_RESULT_REPLY;
}