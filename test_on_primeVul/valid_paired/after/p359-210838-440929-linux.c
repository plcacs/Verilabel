static ssize_t usbip_sockfd_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct stub_device *sdev = dev_get_drvdata(dev);
	int sockfd = 0;
	struct socket *socket;
	int rv;
	struct task_struct *tcp_rx = NULL;
	struct task_struct *tcp_tx = NULL;

	if (!sdev) {
		dev_err(dev, "sdev is null\n");
		return -ENODEV;
	}

	rv = sscanf(buf, "%d", &sockfd);
	if (rv != 1)
		return -EINVAL;

	if (sockfd != -1) {
		int err;

		dev_info(dev, "stub up\n");

		spin_lock_irq(&sdev->ud.lock);

		if (sdev->ud.status != SDEV_ST_AVAILABLE) {
			dev_err(dev, "not ready\n");
			goto err;
		}

		socket = sockfd_lookup(sockfd, &err);
		if (!socket) {
			dev_err(dev, "failed to lookup sock");
			goto err;
		}

		if (socket->type != SOCK_STREAM) {
			dev_err(dev, "Expecting SOCK_STREAM - found %d",
				socket->type);
			goto sock_err;
		}

		/* unlock and create threads and get tasks */
		spin_unlock_irq(&sdev->ud.lock);
		tcp_rx = kthread_create(stub_rx_loop, &sdev->ud, "stub_rx");
		if (IS_ERR(tcp_rx)) {
			sockfd_put(socket);
			return -EINVAL;
		}
		tcp_tx = kthread_create(stub_tx_loop, &sdev->ud, "stub_tx");
		if (IS_ERR(tcp_tx)) {
			kthread_stop(tcp_rx);
			sockfd_put(socket);
			return -EINVAL;
		}

		/* get task structs now */
		get_task_struct(tcp_rx);
		get_task_struct(tcp_tx);

		/* lock and update sdev->ud state */
		spin_lock_irq(&sdev->ud.lock);
		sdev->ud.tcp_socket = socket;
		sdev->ud.sockfd = sockfd;
		sdev->ud.tcp_rx = tcp_rx;
		sdev->ud.tcp_tx = tcp_tx;
		sdev->ud.status = SDEV_ST_USED;
		spin_unlock_irq(&sdev->ud.lock);

		wake_up_process(sdev->ud.tcp_rx);
		wake_up_process(sdev->ud.tcp_tx);

	} else {
		dev_info(dev, "stub down\n");

		spin_lock_irq(&sdev->ud.lock);
		if (sdev->ud.status != SDEV_ST_USED)
			goto err;

		spin_unlock_irq(&sdev->ud.lock);

		usbip_event_add(&sdev->ud, SDEV_EVENT_DOWN);
	}

	return count;

sock_err:
	sockfd_put(socket);
err:
	spin_unlock_irq(&sdev->ud.lock);
	return -EINVAL;
}