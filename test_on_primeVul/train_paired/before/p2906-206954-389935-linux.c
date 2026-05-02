static int keyring_detect_cycle_iterator(const void *object,
					 void *iterator_data)
{
	struct keyring_search_context *ctx = iterator_data;
	const struct key *key = keyring_ptr_to_key(object);

	kenter("{%d}", key->serial);

	BUG_ON(key != ctx->match_data);
	ctx->result = ERR_PTR(-EDEADLK);
	return 1;
}