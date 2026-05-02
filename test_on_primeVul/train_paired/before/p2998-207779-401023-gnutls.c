static int wrap_nettle_hash_fast(gnutls_digest_algorithm_t algo,
				 const void *text, size_t text_size,
				 void *digest)
{
	struct nettle_hash_ctx ctx;
	int ret;

	ret = _ctx_init(algo, &ctx);
	if (ret < 0)
		return gnutls_assert_val(ret);

	ctx.update(&ctx, text_size, text);
	ctx.digest(&ctx, ctx.length, digest);

	return 0;
}