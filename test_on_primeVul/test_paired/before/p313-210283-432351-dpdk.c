vhost_user_set_inflight_fd(struct virtio_net **pdev,
			   struct vhu_msg_context *ctx,
			   int main_fd __rte_unused)
{
	uint64_t mmap_size, mmap_offset;
	uint16_t num_queues, queue_size;
	struct virtio_net *dev = *pdev;
	uint32_t pervq_inflight_size;
	struct vhost_virtqueue *vq;
	void *addr;
	int fd, i;
	int numa_node = SOCKET_ID_ANY;

	fd = ctx->fds[0];
	if (ctx->msg.size != sizeof(ctx->msg.payload.inflight) || fd < 0) {
		VHOST_LOG_CONFIG(ERR, "(%s) invalid set_inflight_fd message size is %d,fd is %d\n",
			dev->ifname, ctx->msg.size, fd);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	mmap_size = ctx->msg.payload.inflight.mmap_size;
	mmap_offset = ctx->msg.payload.inflight.mmap_offset;
	num_queues = ctx->msg.payload.inflight.num_queues;
	queue_size = ctx->msg.payload.inflight.queue_size;

	if (vq_is_packed(dev))
		pervq_inflight_size = get_pervq_shm_size_packed(queue_size);
	else
		pervq_inflight_size = get_pervq_shm_size_split(queue_size);

	VHOST_LOG_CONFIG(INFO, "(%s) set_inflight_fd mmap_size: %"PRIu64"\n",
			dev->ifname, mmap_size);
	VHOST_LOG_CONFIG(INFO, "(%s) set_inflight_fd mmap_offset: %"PRIu64"\n",
			dev->ifname, mmap_offset);
	VHOST_LOG_CONFIG(INFO, "(%s) set_inflight_fd num_queues: %u\n", dev->ifname, num_queues);
	VHOST_LOG_CONFIG(INFO, "(%s) set_inflight_fd queue_size: %u\n", dev->ifname, queue_size);
	VHOST_LOG_CONFIG(INFO, "(%s) set_inflight_fd fd: %d\n", dev->ifname, fd);
	VHOST_LOG_CONFIG(INFO, "(%s) set_inflight_fd pervq_inflight_size: %d\n",
			dev->ifname, pervq_inflight_size);

	/*
	 * If VQ 0 has already been allocated, try to allocate on the same
	 * NUMA node. It can be reallocated later in numa_realloc().
	 */
	if (dev->nr_vring > 0)
		numa_node = dev->virtqueue[0]->numa_node;

	if (!dev->inflight_info) {
		dev->inflight_info = rte_zmalloc_socket("inflight_info",
				sizeof(struct inflight_mem_info), 0, numa_node);
		if (dev->inflight_info == NULL) {
			VHOST_LOG_CONFIG(ERR, "(%s) failed to alloc dev inflight area\n",
					dev->ifname);
			return RTE_VHOST_MSG_RESULT_ERR;
		}
		dev->inflight_info->fd = -1;
	}

	if (dev->inflight_info->addr) {
		munmap(dev->inflight_info->addr, dev->inflight_info->size);
		dev->inflight_info->addr = NULL;
	}

	addr = mmap(0, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    fd, mmap_offset);
	if (addr == MAP_FAILED) {
		VHOST_LOG_CONFIG(ERR, "(%s) failed to mmap share memory.\n", dev->ifname);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (dev->inflight_info->fd >= 0) {
		close(dev->inflight_info->fd);
		dev->inflight_info->fd = -1;
	}

	dev->inflight_info->fd = fd;
	dev->inflight_info->addr = addr;
	dev->inflight_info->size = mmap_size;

	for (i = 0; i < num_queues; i++) {
		vq = dev->virtqueue[i];
		if (!vq)
			continue;

		if (vq_is_packed(dev)) {
			vq->inflight_packed = addr;
			vq->inflight_packed->desc_num = queue_size;
		} else {
			vq->inflight_split = addr;
			vq->inflight_split->desc_num = queue_size;
		}
		addr = (void *)((char *)addr + pervq_inflight_size);
	}

	return RTE_VHOST_MSG_RESULT_OK;
}