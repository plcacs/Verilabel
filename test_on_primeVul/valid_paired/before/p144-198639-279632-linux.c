nosy_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct client *client = file->private_data;
	spinlock_t *client_list_lock = &client->lynx->client_list_lock;
	struct nosy_stats stats;

	switch (cmd) {
	case NOSY_IOC_GET_STATS:
		spin_lock_irq(client_list_lock);
		stats.total_packet_count = client->buffer.total_packet_count;
		stats.lost_packet_count  = client->buffer.lost_packet_count;
		spin_unlock_irq(client_list_lock);

		if (copy_to_user((void __user *) arg, &stats, sizeof stats))
			return -EFAULT;
		else
			return 0;

	case NOSY_IOC_START:
		spin_lock_irq(client_list_lock);
		list_add_tail(&client->link, &client->lynx->client_list);
		spin_unlock_irq(client_list_lock);

		return 0;

	case NOSY_IOC_STOP:
		spin_lock_irq(client_list_lock);
		list_del_init(&client->link);
		spin_unlock_irq(client_list_lock);

		return 0;

	case NOSY_IOC_FILTER:
		spin_lock_irq(client_list_lock);
		client->tcode_mask = arg;
		spin_unlock_irq(client_list_lock);

		return 0;

	default:
		return -EINVAL;
		/* Flush buffer, configure filter. */
	}
}