  explicit HashContext(const HashContext* ctx) {
    assert(ctx->ops);
    assert(ctx->ops->context_size >= 0);
    ops = ctx->ops;
    context = malloc(ops->context_size);
    ops->hash_copy(context, ctx->context);
    options = ctx->options;
    key = ctx->key ? strdup(ctx->key) : nullptr;
  }