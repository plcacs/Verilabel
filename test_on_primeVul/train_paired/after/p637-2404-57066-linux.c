static int snd_seq_device_dev_disconnect(struct snd_device *device)
{
	struct snd_seq_device *dev = device->device_data;

	device_del(&dev->dev);
	return 0;
}
