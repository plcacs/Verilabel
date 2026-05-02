static bool l2cap_check_enc_key_size(struct hci_conn *hcon)
{
	/* The minimum encryption key size needs to be enforced by the
	 * host stack before establishing any L2CAP connections. The
	 * specification in theory allows a minimum of 1, but to align
	 * BR/EDR and LE transports, a minimum of 7 is chosen.
	 *
	 * This check might also be called for unencrypted connections
	 * that have no key size requirements. Ensure that the link is
	 * actually encrypted before enforcing a key size.
	 */
	return (!test_bit(HCI_CONN_ENCRYPT, &hcon->flags) ||
		hcon->enc_key_size > HCI_MIN_ENC_KEY_SIZE);
}