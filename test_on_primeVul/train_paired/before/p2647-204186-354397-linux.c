static ssize_t driver_override_store(struct device *_dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct amba_device *dev = to_amba_device(_dev);
	char *driver_override, *old, *cp;

	if (count > PATH_MAX)
		return -EINVAL;

	driver_override = kstrndup(buf, count, GFP_KERNEL);
	if (!driver_override)
		return -ENOMEM;

	cp = strchr(driver_override, '\n');
	if (cp)
		*cp = '\0';

	device_lock(_dev);
	old = dev->driver_override;
	if (strlen(driver_override)) {
		dev->driver_override = driver_override;
	} else {
	       kfree(driver_override);
	       dev->driver_override = NULL;
	}
	device_unlock(_dev);

	kfree(old);

	return count;
}