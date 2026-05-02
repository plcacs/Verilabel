int st21nfca_connectivity_event_received(struct nfc_hci_dev *hdev, u8 host,
				u8 event, struct sk_buff *skb)
{
	int r = 0;
	struct device *dev = &hdev->ndev->dev;
	struct nfc_evt_transaction *transaction;

	pr_debug("connectivity gate event: %x\n", event);

	switch (event) {
	case ST21NFCA_EVT_CONNECTIVITY:
		r = nfc_se_connectivity(hdev->ndev, host);
	break;
	case ST21NFCA_EVT_TRANSACTION:
		/*
		 * According to specification etsi 102 622
		 * 11.2.2.4 EVT_TRANSACTION Table 52
		 * Description	Tag	Length
		 * AID		81	5 to 16
		 * PARAMETERS	82	0 to 255
		 */
		if (skb->len < NFC_MIN_AID_LENGTH + 2 &&
		    skb->data[0] != NFC_EVT_TRANSACTION_AID_TAG)
			return -EPROTO;

		transaction = devm_kzalloc(dev, skb->len - 2, GFP_KERNEL);
		if (!transaction)
			return -ENOMEM;

		transaction->aid_len = skb->data[1];
		memcpy(transaction->aid, &skb->data[2],
		       transaction->aid_len);

		/* Check next byte is PARAMETERS tag (82) */
		if (skb->data[transaction->aid_len + 2] !=
		    NFC_EVT_TRANSACTION_PARAMS_TAG)
			return -EPROTO;

		transaction->params_len = skb->data[transaction->aid_len + 3];
		memcpy(transaction->params, skb->data +
		       transaction->aid_len + 4, transaction->params_len);

		r = nfc_se_transaction(hdev->ndev, host, transaction);
	break;
	default:
		nfc_err(&hdev->ndev->dev, "Unexpected event on connectivity gate\n");
		return 1;
	}
	kfree_skb(skb);
	return r;
}