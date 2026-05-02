static int ax88179_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	struct sk_buff *ax_skb;
	int pkt_cnt;
	u32 rx_hdr;
	u16 hdr_off;
	u32 *pkt_hdr;

	/* At the end of the SKB, there's a header telling us how many packets
	 * are bundled into this buffer and where we can find an array of
	 * per-packet metadata (which contains elements encoded into u16).
	 */
	if (skb->len < 4)
		return 0;
	skb_trim(skb, skb->len - 4);
	rx_hdr = get_unaligned_le32(skb_tail_pointer(skb));
	pkt_cnt = (u16)rx_hdr;
	hdr_off = (u16)(rx_hdr >> 16);

	if (pkt_cnt == 0)
		return 0;

	/* Make sure that the bounds of the metadata array are inside the SKB
	 * (and in front of the counter at the end).
	 */
	if (pkt_cnt * 2 + hdr_off > skb->len)
		return 0;
	pkt_hdr = (u32 *)(skb->data + hdr_off);

	/* Packets must not overlap the metadata array */
	skb_trim(skb, hdr_off);

	for (; ; pkt_cnt--, pkt_hdr++) {
		u16 pkt_len;

		le32_to_cpus(pkt_hdr);
		pkt_len = (*pkt_hdr >> 16) & 0x1fff;

		if (pkt_len > skb->len)
			return 0;

		/* Check CRC or runt packet */
		if (((*pkt_hdr & (AX_RXHDR_CRC_ERR | AX_RXHDR_DROP_ERR)) == 0) &&
		    pkt_len >= 2 + ETH_HLEN) {
			bool last = (pkt_cnt == 0);

			if (last) {
				ax_skb = skb;
			} else {
				ax_skb = skb_clone(skb, GFP_ATOMIC);
				if (!ax_skb)
					return 0;
			}
			ax_skb->len = pkt_len;
			/* Skip IP alignment pseudo header */
			skb_pull(ax_skb, 2);
			skb_set_tail_pointer(ax_skb, ax_skb->len);
			ax_skb->truesize = pkt_len + sizeof(struct sk_buff);
			ax88179_rx_checksum(ax_skb, pkt_hdr);

			if (last)
				return 1;

			usbnet_skb_return(dev, ax_skb);
		}

		/* Trim this packet away from the SKB */
		if (!skb_pull(skb, (pkt_len + 7) & 0xFFF8))
			return 0;
	}
}