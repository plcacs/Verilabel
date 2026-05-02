static int list_devices(struct file *filp, struct dm_ioctl *param, size_t param_size)
{
	unsigned int i;
	struct hash_cell *hc;
	size_t len, needed = 0;
	struct gendisk *disk;
	struct dm_name_list *orig_nl, *nl, *old_nl = NULL;
	uint32_t *event_nr;

	down_write(&_hash_lock);

	/*
	 * Loop through all the devices working out how much
	 * space we need.
	 */
	for (i = 0; i < NUM_BUCKETS; i++) {
		list_for_each_entry (hc, _name_buckets + i, name_list) {
			needed += align_val(offsetof(struct dm_name_list, name) + strlen(hc->name) + 1);
			needed += align_val(sizeof(uint32_t));
		}
	}

	/*
	 * Grab our output buffer.
	 */
	nl = orig_nl = get_result_buffer(param, param_size, &len);
	if (len < needed) {
		param->flags |= DM_BUFFER_FULL_FLAG;
		goto out;
	}
	param->data_size = param->data_start + needed;

	nl->dev = 0;	/* Flags no data */

	/*
	 * Now loop through filling out the names.
	 */
	for (i = 0; i < NUM_BUCKETS; i++) {
		list_for_each_entry (hc, _name_buckets + i, name_list) {
			if (old_nl)
				old_nl->next = (uint32_t) ((void *) nl -
							   (void *) old_nl);
			disk = dm_disk(hc->md);
			nl->dev = huge_encode_dev(disk_devt(disk));
			nl->next = 0;
			strcpy(nl->name, hc->name);

			old_nl = nl;
			event_nr = align_ptr(nl->name + strlen(hc->name) + 1);
			*event_nr = dm_get_event_nr(hc->md);
			nl = align_ptr(event_nr + 1);
		}
	}
	/*
	 * If mismatch happens, security may be compromised due to buffer
	 * overflow, so it's better to crash.
	 */
	BUG_ON((char *)nl - (char *)orig_nl != needed);

 out:
	up_write(&_hash_lock);
	return 0;
}