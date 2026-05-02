virtio_dev_rx_batch_packed(struct virtio_net *dev,
			   struct vhost_virtqueue *vq,
			   struct rte_mbuf **pkts)
{
	bool wrap_counter = vq->avail_wrap_counter;
	struct vring_packed_desc *descs = vq->desc_packed;
	uint16_t avail_idx = vq->last_avail_idx;
	uint64_t desc_addrs[PACKED_BATCH_SIZE];
	struct virtio_net_hdr_mrg_rxbuf *hdrs[PACKED_BATCH_SIZE];
	uint32_t buf_offset = dev->vhost_hlen;
	uint64_t lens[PACKED_BATCH_SIZE];
	uint16_t ids[PACKED_BATCH_SIZE];
	uint16_t i;

	if (unlikely(avail_idx & PACKED_BATCH_MASK))
		return -1;

	if (unlikely((avail_idx + PACKED_BATCH_SIZE) > vq->size))
		return -1;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(pkts[i]->next != NULL))
			return -1;
		if (unlikely(!desc_is_avail(&descs[avail_idx + i],
					    wrap_counter)))
			return -1;
	}

	rte_smp_rmb();

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		lens[i] = descs[avail_idx + i].len;

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(pkts[i]->pkt_len > (lens[i] - buf_offset)))
			return -1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		desc_addrs[i] = vhost_iova_to_vva(dev, vq,
						  descs[avail_idx + i].addr,
						  &lens[i],
						  VHOST_ACCESS_RW);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		if (unlikely(lens[i] != descs[avail_idx + i].len))
			return -1;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		rte_prefetch0((void *)(uintptr_t)desc_addrs[i]);
		hdrs[i] = (struct virtio_net_hdr_mrg_rxbuf *)
					(uintptr_t)desc_addrs[i];
		lens[i] = pkts[i]->pkt_len + dev->vhost_hlen;
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		virtio_enqueue_offload(pkts[i], &hdrs[i]->hdr);

	vq_inc_last_avail_packed(vq, PACKED_BATCH_SIZE);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		rte_memcpy((void *)(uintptr_t)(desc_addrs[i] + buf_offset),
			   rte_pktmbuf_mtod_offset(pkts[i], void *, 0),
			   pkts[i]->pkt_len);
	}

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		vhost_log_cache_write_iova(dev, vq, descs[avail_idx + i].addr,
					   lens[i]);

	vhost_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE)
		ids[i] = descs[avail_idx + i].id;

	vhost_flush_enqueue_batch_packed(dev, vq, lens, ids);

	return 0;
}