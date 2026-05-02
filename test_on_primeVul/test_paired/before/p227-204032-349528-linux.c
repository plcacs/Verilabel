static void virtbt_rx_handle(struct virtio_bluetooth *vbt, struct sk_buff *skb)
{
	__u8 pkt_type;

	pkt_type = *((__u8 *) skb->data);
	skb_pull(skb, 1);

	switch (pkt_type) {
	case HCI_EVENT_PKT:
	case HCI_ACLDATA_PKT:
	case HCI_SCODATA_PKT:
	case HCI_ISODATA_PKT:
		hci_skb_pkt_type(skb) = pkt_type;
		hci_recv_frame(vbt->hdev, skb);
		break;
	}
}