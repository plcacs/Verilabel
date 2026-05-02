  explicit HashContext(const HashContext* ctx) {
    assert(ctx->ops);
    assert(ctx->ops->context_size >= 0);
    ops = ctx->ops;
    context = malloc(ops->context_size);
    ops->hash_copy(context, ctx->context);
    options = ctx->options;
    if (ctx->key) {
      key = static_cast<char*>(malloc(ops->block_size));
      memcpy(key, ctx->key, ops->block_size);
    } else {
      key = nullptr;
    }
  }