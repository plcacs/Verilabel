static bool tipc_crypto_key_rcv(struct tipc_crypto *rx, struct tipc_msg *hdr)
{
	struct tipc_crypto *tx = tipc_net(rx->net)->crypto_tx;
	struct tipc_aead_key *skey = NULL;
	u16 key_gen = msg_key_gen(hdr);
	u16 size = msg_data_sz(hdr);
	u8 *data = msg_data(hdr);
	unsigned int keylen;

	/* Verify whether the size can exist in the packet */
	if (unlikely(size < sizeof(struct tipc_aead_key) + TIPC_AEAD_KEYLEN_MIN)) {
		pr_debug("%s: message data size is too small\n", rx->name);
		goto exit;
	}

	keylen = ntohl(*((__be32 *)(data + TIPC_AEAD_ALG_NAME)));

	/* Verify the supplied size values */
	if (unlikely(size != keylen + sizeof(struct tipc_aead_key) ||
		     keylen > TIPC_AEAD_KEY_SIZE_MAX)) {
		pr_debug("%s: invalid MSG_CRYPTO key size\n", rx->name);
		goto exit;
	}

	spin_lock(&rx->lock);
	if (unlikely(rx->skey || (key_gen == rx->key_gen && rx->key.keys))) {
		pr_err("%s: key existed <%p>, gen %d vs %d\n", rx->name,
		       rx->skey, key_gen, rx->key_gen);
		goto exit_unlock;
	}

	/* Allocate memory for the key */
	skey = kmalloc(size, GFP_ATOMIC);
	if (unlikely(!skey)) {
		pr_err("%s: unable to allocate memory for skey\n", rx->name);
		goto exit_unlock;
	}

	/* Copy key from msg data */
	skey->keylen = keylen;
	memcpy(skey->alg_name, data, TIPC_AEAD_ALG_NAME);
	memcpy(skey->key, data + TIPC_AEAD_ALG_NAME + sizeof(__be32),
	       skey->keylen);

	rx->key_gen = key_gen;
	rx->skey_mode = msg_key_mode(hdr);
	rx->skey = skey;
	rx->nokey = 0;
	mb(); /* for nokey flag */

exit_unlock:
	spin_unlock(&rx->lock);

exit:
	/* Schedule the key attaching on this crypto */
	if (likely(skey && queue_delayed_work(tx->wq, &rx->work, 0)))
		return true;

	return false;
}