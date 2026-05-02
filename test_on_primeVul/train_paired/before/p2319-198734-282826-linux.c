static void sp_setup(struct net_device *dev)
{
	/* Finish setting up the DEVICE info. */
	dev->netdev_ops		= &sp_netdev_ops;
	dev->needs_free_netdev	= true;
	dev->mtu		= SIXP_MTU;
	dev->hard_header_len	= AX25_MAX_HEADER_LEN;
	dev->header_ops 	= &ax25_header_ops;

	dev->addr_len		= AX25_ADDR_LEN;
	dev->type		= ARPHRD_AX25;
	dev->tx_queue_len	= 10;

	/* Only activated in AX.25 mode */
	memcpy(dev->broadcast, &ax25_bcast, AX25_ADDR_LEN);
	dev_addr_set(dev, (u8 *)&ax25_defaddr);

	dev->flags		= 0;
}