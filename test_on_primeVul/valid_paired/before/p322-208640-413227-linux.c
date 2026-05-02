mt76_add_fragment(struct mt76_dev *dev, struct mt76_queue *q, void *data,
		  int len, bool more)
{
	struct page *page = virt_to_head_page(data);
	int offset = data - page_address(page);
	struct sk_buff *skb = q->rx_head;

	offset += q->buf_offset;
	skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags, page, offset, len,
			q->buf_size);

	if (more)
		return;

	q->rx_head = NULL;
	dev->drv->rx_skb(dev, q - dev->q_rx, skb);
}