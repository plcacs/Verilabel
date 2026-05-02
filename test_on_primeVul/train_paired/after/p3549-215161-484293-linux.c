static ssize_t gadget_dev_desc_UDC_store(struct config_item *item,
		const char *page, size_t len)
{
	struct gadget_info *gi = to_gadget_info(item);
	char *name;
	int ret;

	if (strlen(page) < len)
		return -EOVERFLOW;

	name = kstrdup(page, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	if (name[len - 1] == '\n')
		name[len - 1] = '\0';

	mutex_lock(&gi->lock);

	if (!strlen(name)) {
		ret = unregister_gadget(gi);
		if (ret)
			goto err;
		kfree(name);
	} else {
		if (gi->composite.gadget_driver.udc_name) {
			ret = -EBUSY;
			goto err;
		}
		gi->composite.gadget_driver.udc_name = name;
		ret = usb_gadget_probe_driver(&gi->composite.gadget_driver);
		if (ret) {
			gi->composite.gadget_driver.udc_name = NULL;
			goto err;
		}
	}
	mutex_unlock(&gi->lock);
	return len;
err:
	kfree(name);
	mutex_unlock(&gi->lock);
	return ret;
}