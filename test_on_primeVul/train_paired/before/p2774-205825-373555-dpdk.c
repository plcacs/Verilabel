vhost_user_get_inflight_fd(struct virtio_net **pdev,
			   VhostUserMsg *msg,
			   int main_fd __rte_unused)
{
	struct rte_vhost_inflight_info_packed *inflight_packed;
	uint64_t pervq_inflight_size, mmap_size;
	uint16_t num_queues, queue_size;
	struct virtio_net *dev = *pdev;
	int fd, i, j;
	void *addr;

	if (msg->size != sizeof(msg->payload.inflight)) {
		VHOST_LOG_CONFIG(ERR,
			"invalid get_inflight_fd message size is %d\n",
			msg->size);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (dev->inflight_info == NULL) {
		dev->inflight_info = calloc(1,
					    sizeof(struct inflight_mem_info));
		if (!dev->inflight_info) {
			VHOST_LOG_CONFIG(ERR,
				"failed to alloc dev inflight area\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	}

	num_queues = msg->payload.inflight.num_queues;
	queue_size = msg->payload.inflight.queue_size;

	VHOST_LOG_CONFIG(INFO, "get_inflight_fd num_queues: %u\n",
		msg->payload.inflight.num_queues);
	VHOST_LOG_CONFIG(INFO, "get_inflight_fd queue_size: %u\n",
		msg->payload.inflight.queue_size);

	if (vq_is_packed(dev))
		pervq_inflight_size = get_pervq_shm_size_packed(queue_size);
	else
		pervq_inflight_size = get_pervq_shm_size_split(queue_size);

	mmap_size = num_queues * pervq_inflight_size;
	addr = inflight_mem_alloc("vhost-inflight", mmap_size, &fd);
	if (!addr) {
		VHOST_LOG_CONFIG(ERR,
			"failed to alloc vhost inflight area\n");
			msg->payload.inflight.mmap_size = 0;
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	memset(addr, 0, mmap_size);

	if (dev->inflight_info->addr) {
		munmap(dev->inflight_info->addr, dev->inflight_info->size);
		dev->inflight_info->addr = NULL;
	}

	dev->inflight_info->addr = addr;
	dev->inflight_info->size = msg->payload.inflight.mmap_size = mmap_size;
	dev->inflight_info->fd = msg->fds[0] = fd;
	msg->payload.inflight.mmap_offset = 0;
	msg->fd_num = 1;

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

	VHOST_LOG_CONFIG(INFO,
		"send inflight mmap_size: %"PRIu64"\n",
		msg->payload.inflight.mmap_size);
	VHOST_LOG_CONFIG(INFO,
		"send inflight mmap_offset: %"PRIu64"\n",
		msg->payload.inflight.mmap_offset);
	VHOST_LOG_CONFIG(INFO,
		"send inflight fd: %d\n", msg->fds[0]);

	return RTE_VHOST_MSG_RESULT_REPLY;
}