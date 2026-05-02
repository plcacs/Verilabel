static int xemaclite_of_probe(struct platform_device *ofdev)
{
	struct resource *res;
	struct net_device *ndev = NULL;
	struct net_local *lp = NULL;
	struct device *dev = &ofdev->dev;

	int rc = 0;

	dev_info(dev, "Device Tree Probing\n");

	/* Create an ethernet device instance */
	ndev = alloc_etherdev(sizeof(struct net_local));
	if (!ndev)
		return -ENOMEM;

	dev_set_drvdata(dev, ndev);
	SET_NETDEV_DEV(ndev, &ofdev->dev);

	lp = netdev_priv(ndev);
	lp->ndev = ndev;

	/* Get IRQ for the device */
	res = platform_get_resource(ofdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "no IRQ found\n");
		rc = -ENXIO;
		goto error;
	}

	ndev->irq = res->start;

	res = platform_get_resource(ofdev, IORESOURCE_MEM, 0);
	lp->base_addr = devm_ioremap_resource(&ofdev->dev, res);
	if (IS_ERR(lp->base_addr)) {
		rc = PTR_ERR(lp->base_addr);
		goto error;
	}

	ndev->mem_start = res->start;
	ndev->mem_end = res->end;

	spin_lock_init(&lp->reset_lock);
	lp->next_tx_buf_to_use = 0x0;
	lp->next_rx_buf_to_use = 0x0;
	lp->tx_ping_pong = get_bool(ofdev, "xlnx,tx-ping-pong");
	lp->rx_ping_pong = get_bool(ofdev, "xlnx,rx-ping-pong");

	rc = of_get_mac_address(ofdev->dev.of_node, ndev->dev_addr);
	if (rc) {
		dev_warn(dev, "No MAC address found, using random\n");
		eth_hw_addr_random(ndev);
	}

	/* Clear the Tx CSR's in case this is a restart */
	xemaclite_writel(0, lp->base_addr + XEL_TSR_OFFSET);
	xemaclite_writel(0, lp->base_addr + XEL_BUFFER_OFFSET + XEL_TSR_OFFSET);

	/* Set the MAC address in the EmacLite device */
	xemaclite_update_address(lp, ndev->dev_addr);

	lp->phy_node = of_parse_phandle(ofdev->dev.of_node, "phy-handle", 0);
	xemaclite_mdio_setup(lp, &ofdev->dev);

	dev_info(dev, "MAC address is now %pM\n", ndev->dev_addr);

	ndev->netdev_ops = &xemaclite_netdev_ops;
	ndev->ethtool_ops = &xemaclite_ethtool_ops;
	ndev->flags &= ~IFF_MULTICAST;
	ndev->watchdog_timeo = TX_TIMEOUT;

	/* Finally, register the device */
	rc = register_netdev(ndev);
	if (rc) {
		dev_err(dev,
			"Cannot register network device, aborting\n");
		goto error;
	}

	dev_info(dev,
		 "Xilinx EmacLite at 0x%08lX mapped to 0x%p, irq=%d\n",
		 (unsigned long __force)ndev->mem_start, lp->base_addr, ndev->irq);
	return 0;

error:
	free_netdev(ndev);
	return rc;
}