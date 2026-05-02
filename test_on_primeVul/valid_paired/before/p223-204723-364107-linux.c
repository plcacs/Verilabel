static int xdp_umem_reg(struct xdp_umem *umem, struct xdp_umem_reg *mr)
{
	bool unaligned_chunks = mr->flags & XDP_UMEM_UNALIGNED_CHUNK_FLAG;
	u32 chunk_size = mr->chunk_size, headroom = mr->headroom;
	unsigned int chunks, chunks_per_page;
	u64 addr = mr->addr, size = mr->len;
	int size_chk, err;

	if (chunk_size < XDP_UMEM_MIN_CHUNK_SIZE || chunk_size > PAGE_SIZE) {
		/* Strictly speaking we could support this, if:
		 * - huge pages, or*
		 * - using an IOMMU, or
		 * - making sure the memory area is consecutive
		 * but for now, we simply say "computer says no".
		 */
		return -EINVAL;
	}

	if (mr->flags & ~(XDP_UMEM_UNALIGNED_CHUNK_FLAG |
			XDP_UMEM_USES_NEED_WAKEUP))
		return -EINVAL;

	if (!unaligned_chunks && !is_power_of_2(chunk_size))
		return -EINVAL;

	if (!PAGE_ALIGNED(addr)) {
		/* Memory area has to be page size aligned. For
		 * simplicity, this might change.
		 */
		return -EINVAL;
	}

	if ((addr + size) < addr)
		return -EINVAL;

	chunks = (unsigned int)div_u64(size, chunk_size);
	if (chunks == 0)
		return -EINVAL;

	if (!unaligned_chunks) {
		chunks_per_page = PAGE_SIZE / chunk_size;
		if (chunks < chunks_per_page || chunks % chunks_per_page)
			return -EINVAL;
	}

	size_chk = chunk_size - headroom - XDP_PACKET_HEADROOM;
	if (size_chk < 0)
		return -EINVAL;

	umem->address = (unsigned long)addr;
	umem->chunk_mask = unaligned_chunks ? XSK_UNALIGNED_BUF_ADDR_MASK
					    : ~((u64)chunk_size - 1);
	umem->size = size;
	umem->headroom = headroom;
	umem->chunk_size_nohr = chunk_size - headroom;
	umem->npgs = size / PAGE_SIZE;
	umem->pgs = NULL;
	umem->user = NULL;
	umem->flags = mr->flags;
	INIT_LIST_HEAD(&umem->xsk_list);
	spin_lock_init(&umem->xsk_list_lock);

	refcount_set(&umem->users, 1);

	err = xdp_umem_account_pages(umem);
	if (err)
		return err;

	err = xdp_umem_pin_pages(umem);
	if (err)
		goto out_account;

	umem->pages = kvcalloc(umem->npgs, sizeof(*umem->pages),
			       GFP_KERNEL_ACCOUNT);
	if (!umem->pages) {
		err = -ENOMEM;
		goto out_pin;
	}

	err = xdp_umem_map_pages(umem);
	if (!err)
		return 0;

	kvfree(umem->pages);

out_pin:
	xdp_umem_unpin_pages(umem);
out_account:
	xdp_umem_unaccount_pages(umem);
	return err;
}