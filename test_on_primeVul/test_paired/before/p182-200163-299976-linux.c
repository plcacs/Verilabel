static int elo_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct elo_priv *priv;
	int ret;
	struct usb_device *udev;

	if (!hid_is_usb(hdev))
		return -EINVAL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	INIT_DELAYED_WORK(&priv->work, elo_work);
	udev = interface_to_usbdev(to_usb_interface(hdev->dev.parent));
	priv->usbdev = usb_get_dev(udev);

	hid_set_drvdata(hdev, priv);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	if (elo_broken_firmware(priv->usbdev)) {
		hid_info(hdev, "broken firmware found, installing workaround\n");
		queue_delayed_work(wq, &priv->work, ELO_PERIODIC_READ_INTERVAL);
	}

	return 0;
err_free:
	kfree(priv);
	return ret;
}