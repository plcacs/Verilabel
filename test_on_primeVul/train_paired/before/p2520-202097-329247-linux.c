static int acpi_smbus_hc_add(struct acpi_device *device)
{
	int status;
	unsigned long long val;
	struct acpi_smb_hc *hc;

	if (!device)
		return -EINVAL;

	status = acpi_evaluate_integer(device->handle, "_EC", NULL, &val);
	if (ACPI_FAILURE(status)) {
		printk(KERN_ERR PREFIX "error obtaining _EC.\n");
		return -EIO;
	}

	strcpy(acpi_device_name(device), ACPI_SMB_HC_DEVICE_NAME);
	strcpy(acpi_device_class(device), ACPI_SMB_HC_CLASS);

	hc = kzalloc(sizeof(struct acpi_smb_hc), GFP_KERNEL);
	if (!hc)
		return -ENOMEM;
	mutex_init(&hc->lock);
	init_waitqueue_head(&hc->wait);

	hc->ec = acpi_driver_data(device->parent);
	hc->offset = (val >> 8) & 0xff;
	hc->query_bit = val & 0xff;
	device->driver_data = hc;

	acpi_ec_add_query_handler(hc->ec, hc->query_bit, NULL, smbus_alarm, hc);
	printk(KERN_INFO PREFIX "SBS HC: EC = 0x%p, offset = 0x%0x, query_bit = 0x%0x\n",
		hc->ec, hc->offset, hc->query_bit);

	return 0;
}