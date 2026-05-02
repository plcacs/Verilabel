dev_config (struct file *fd, const char __user *buf, size_t len, loff_t *ptr)
{
	struct dev_data		*dev = fd->private_data;
	ssize_t			value, length = len;
	unsigned		total;
	u32			tag;
	char			*kbuf;

	spin_lock_irq(&dev->lock);
	if (dev->state > STATE_DEV_OPENED) {
		value = ep0_write(fd, buf, len, ptr);
		spin_unlock_irq(&dev->lock);
		return value;
	}
	spin_unlock_irq(&dev->lock);

	if ((len < (USB_DT_CONFIG_SIZE + USB_DT_DEVICE_SIZE + 4)) ||
	    (len > PAGE_SIZE * 4))
		return -EINVAL;

	/* we might need to change message format someday */
	if (copy_from_user (&tag, buf, 4))
		return -EFAULT;
	if (tag != 0)
		return -EINVAL;
	buf += 4;
	length -= 4;

	kbuf = memdup_user(buf, length);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	spin_lock_irq (&dev->lock);
	value = -EINVAL;
	if (dev->buf) {
		spin_unlock_irq(&dev->lock);
		kfree(kbuf);
		return value;
	}
	dev->buf = kbuf;

	/* full or low speed config */
	dev->config = (void *) kbuf;
	total = le16_to_cpu(dev->config->wTotalLength);
	if (!is_valid_config(dev->config, total) ||
			total > length - USB_DT_DEVICE_SIZE)
		goto fail;
	kbuf += total;
	length -= total;

	/* optional high speed config */
	if (kbuf [1] == USB_DT_CONFIG) {
		dev->hs_config = (void *) kbuf;
		total = le16_to_cpu(dev->hs_config->wTotalLength);
		if (!is_valid_config(dev->hs_config, total) ||
				total > length - USB_DT_DEVICE_SIZE)
			goto fail;
		kbuf += total;
		length -= total;
	} else {
		dev->hs_config = NULL;
	}

	/* could support multiple configs, using another encoding! */

	/* device descriptor (tweaked for paranoia) */
	if (length != USB_DT_DEVICE_SIZE)
		goto fail;
	dev->dev = (void *)kbuf;
	if (dev->dev->bLength != USB_DT_DEVICE_SIZE
			|| dev->dev->bDescriptorType != USB_DT_DEVICE
			|| dev->dev->bNumConfigurations != 1)
		goto fail;
	dev->dev->bcdUSB = cpu_to_le16 (0x0200);

	/* triggers gadgetfs_bind(); then we can enumerate. */
	spin_unlock_irq (&dev->lock);
	if (dev->hs_config)
		gadgetfs_driver.max_speed = USB_SPEED_HIGH;
	else
		gadgetfs_driver.max_speed = USB_SPEED_FULL;

	value = usb_gadget_probe_driver(&gadgetfs_driver);
	if (value != 0) {
		kfree (dev->buf);
		dev->buf = NULL;
	} else {
		/* at this point "good" hardware has for the first time
		 * let the USB the host see us.  alternatively, if users
		 * unplug/replug that will clear all the error state.
		 *
		 * note:  everything running before here was guaranteed
		 * to choke driver model style diagnostics.  from here
		 * on, they can work ... except in cleanup paths that
		 * kick in after the ep0 descriptor is closed.
		 */
		value = len;
		dev->gadget_registered = true;
	}
	return value;

fail:
	spin_unlock_irq (&dev->lock);
	pr_debug ("%s: %s fail %zd, %p\n", shortname, __func__, value, dev);
	kfree (dev->buf);
	dev->buf = NULL;
	return value;
}