static void hci_extended_inquiry_result_evt(struct hci_dev *hdev,
					    struct sk_buff *skb)
{
	struct inquiry_data data;
	struct extended_inquiry_info *info = (void *) (skb->data + 1);
	int num_rsp = *((__u8 *) skb->data);
	size_t eir_len;

	BT_DBG("%s num_rsp %d", hdev->name, num_rsp);

	if (!num_rsp)
		return;

	if (hci_dev_test_flag(hdev, HCI_PERIODIC_INQ))
		return;

	hci_dev_lock(hdev);

	for (; num_rsp; num_rsp--, info++) {
		u32 flags;
		bool name_known;

		bacpy(&data.bdaddr, &info->bdaddr);
		data.pscan_rep_mode	= info->pscan_rep_mode;
		data.pscan_period_mode	= info->pscan_period_mode;
		data.pscan_mode		= 0x00;
		memcpy(data.dev_class, info->dev_class, 3);
		data.clock_offset	= info->clock_offset;
		data.rssi		= info->rssi;
		data.ssp_mode		= 0x01;

		if (hci_dev_test_flag(hdev, HCI_MGMT))
			name_known = eir_get_data(info->data,
						  sizeof(info->data),
						  EIR_NAME_COMPLETE, NULL);
		else
			name_known = true;

		flags = hci_inquiry_cache_update(hdev, &data, name_known);

		eir_len = eir_get_length(info->data, sizeof(info->data));

		mgmt_device_found(hdev, &info->bdaddr, ACL_LINK, 0x00,
				  info->dev_class, info->rssi,
				  flags, info->data, eir_len, NULL, 0);
	}

	hci_dev_unlock(hdev);
}