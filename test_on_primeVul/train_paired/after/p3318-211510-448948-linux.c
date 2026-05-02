temac_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct temac_local *lp = netdev_priv(ndev);
	struct cdmac_bd *cur_p;
	dma_addr_t tail_p, skb_dma_addr;
	int ii;
	unsigned long num_frag;
	skb_frag_t *frag;

	num_frag = skb_shinfo(skb)->nr_frags;
	frag = &skb_shinfo(skb)->frags[0];
	cur_p = &lp->tx_bd_v[lp->tx_bd_tail];

	if (temac_check_tx_bd_space(lp, num_frag + 1)) {
		if (netif_queue_stopped(ndev))
			return NETDEV_TX_BUSY;

		netif_stop_queue(ndev);

		/* Matches barrier in temac_start_xmit_done */
		smp_mb();

		/* Space might have just been freed - check again */
		if (temac_check_tx_bd_space(lp, num_frag + 1))
			return NETDEV_TX_BUSY;

		netif_wake_queue(ndev);
	}

	cur_p->app0 = 0;
	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		unsigned int csum_start_off = skb_checksum_start_offset(skb);
		unsigned int csum_index_off = csum_start_off + skb->csum_offset;

		cur_p->app0 |= cpu_to_be32(0x000001); /* TX Checksum Enabled */
		cur_p->app1 = cpu_to_be32((csum_start_off << 16)
					  | csum_index_off);
		cur_p->app2 = 0;  /* initial checksum seed */
	}

	cur_p->app0 |= cpu_to_be32(STS_CTRL_APP0_SOP);
	skb_dma_addr = dma_map_single(ndev->dev.parent, skb->data,
				      skb_headlen(skb), DMA_TO_DEVICE);
	cur_p->len = cpu_to_be32(skb_headlen(skb));
	if (WARN_ON_ONCE(dma_mapping_error(ndev->dev.parent, skb_dma_addr))) {
		dev_kfree_skb_any(skb);
		ndev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}
	cur_p->phys = cpu_to_be32(skb_dma_addr);

	for (ii = 0; ii < num_frag; ii++) {
		if (++lp->tx_bd_tail >= lp->tx_bd_num)
			lp->tx_bd_tail = 0;

		cur_p = &lp->tx_bd_v[lp->tx_bd_tail];
		skb_dma_addr = dma_map_single(ndev->dev.parent,
					      skb_frag_address(frag),
					      skb_frag_size(frag),
					      DMA_TO_DEVICE);
		if (dma_mapping_error(ndev->dev.parent, skb_dma_addr)) {
			if (--lp->tx_bd_tail < 0)
				lp->tx_bd_tail = lp->tx_bd_num - 1;
			cur_p = &lp->tx_bd_v[lp->tx_bd_tail];
			while (--ii >= 0) {
				--frag;
				dma_unmap_single(ndev->dev.parent,
						 be32_to_cpu(cur_p->phys),
						 skb_frag_size(frag),
						 DMA_TO_DEVICE);
				if (--lp->tx_bd_tail < 0)
					lp->tx_bd_tail = lp->tx_bd_num - 1;
				cur_p = &lp->tx_bd_v[lp->tx_bd_tail];
			}
			dma_unmap_single(ndev->dev.parent,
					 be32_to_cpu(cur_p->phys),
					 skb_headlen(skb), DMA_TO_DEVICE);
			dev_kfree_skb_any(skb);
			ndev->stats.tx_dropped++;
			return NETDEV_TX_OK;
		}
		cur_p->phys = cpu_to_be32(skb_dma_addr);
		cur_p->len = cpu_to_be32(skb_frag_size(frag));
		cur_p->app0 = 0;
		frag++;
	}
	cur_p->app0 |= cpu_to_be32(STS_CTRL_APP0_EOP);

	/* Mark last fragment with skb address, so it can be consumed
	 * in temac_start_xmit_done()
	 */
	ptr_to_txbd((void *)skb, cur_p);

	tail_p = lp->tx_bd_p + sizeof(*lp->tx_bd_v) * lp->tx_bd_tail;
	lp->tx_bd_tail++;
	if (lp->tx_bd_tail >= lp->tx_bd_num)
		lp->tx_bd_tail = 0;

	skb_tx_timestamp(skb);

	/* Kick off the transfer */
	wmb();
	lp->dma_out(lp, TX_TAILDESC_PTR, tail_p); /* DMA start */

	return NETDEV_TX_OK;
}