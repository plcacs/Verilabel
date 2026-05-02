static netdev_tx_t mcba_usb_start_xmit(struct sk_buff *skb,
				       struct net_device *netdev)
{
	struct mcba_priv *priv = netdev_priv(netdev);
	struct can_frame *cf = (struct can_frame *)skb->data;
	struct mcba_usb_ctx *ctx = NULL;
	struct net_device_stats *stats = &priv->netdev->stats;
	u16 sid;
	int err;
	struct mcba_usb_msg_can usb_msg = {
		.cmd_id = MBCA_CMD_TRANSMIT_MESSAGE_EV
	};

	if (can_dropped_invalid_skb(netdev, skb))
		return NETDEV_TX_OK;

	ctx = mcba_usb_get_free_ctx(priv, cf);
	if (!ctx)
		return NETDEV_TX_BUSY;

	if (cf->can_id & CAN_EFF_FLAG) {
		/* SIDH    | SIDL                 | EIDH   | EIDL
		 * 28 - 21 | 20 19 18 x x x 17 16 | 15 - 8 | 7 - 0
		 */
		sid = MCBA_SIDL_EXID_MASK;
		/* store 28-18 bits */
		sid |= (cf->can_id & 0x1ffc0000) >> 13;
		/* store 17-16 bits */
		sid |= (cf->can_id & 0x30000) >> 16;
		put_unaligned_be16(sid, &usb_msg.sid);

		/* store 15-0 bits */
		put_unaligned_be16(cf->can_id & 0xffff, &usb_msg.eid);
	} else {
		/* SIDH   | SIDL
		 * 10 - 3 | 2 1 0 x x x x x
		 */
		put_unaligned_be16((cf->can_id & CAN_SFF_MASK) << 5,
				   &usb_msg.sid);
		usb_msg.eid = 0;
	}

	usb_msg.dlc = cf->len;

	memcpy(usb_msg.data, cf->data, usb_msg.dlc);

	if (cf->can_id & CAN_RTR_FLAG)
		usb_msg.dlc |= MCBA_DLC_RTR_MASK;

	can_put_echo_skb(skb, priv->netdev, ctx->ndx, 0);

	err = mcba_usb_xmit(priv, (struct mcba_usb_msg *)&usb_msg, ctx);
	if (err)
		goto xmit_failed;

	return NETDEV_TX_OK;

xmit_failed:
	can_free_echo_skb(priv->netdev, ctx->ndx, NULL);
	mcba_usb_free_ctx(ctx);
	dev_kfree_skb(skb);
	stats->tx_dropped++;

	return NETDEV_TX_OK;
}