  void Compute(OpKernelContext* ctx) override {
    StagingMap<Ordered>* map = nullptr;
    OP_REQUIRES_OK(ctx, GetStagingMap(ctx, def(), &map));
    core::ScopedUnref scope(map);
    typename StagingMap<Ordered>::OptionalTuple tuple;

    const Tensor* key_tensor;
    const Tensor* indices_tensor;
    OpInputList values_tensor;

    OP_REQUIRES_OK(ctx, ctx->input("key", &key_tensor));
    OP_REQUIRES_OK(ctx, ctx->input("indices", &indices_tensor));
    OP_REQUIRES_OK(ctx, ctx->input_list("values", &values_tensor));
    OP_REQUIRES(ctx, key_tensor->NumElements() > 0,
                errors::InvalidArgument("key must not be empty"));

    OP_REQUIRES(ctx, key_tensor->NumElements() == 1,
                errors::InvalidArgument(
                    "key must be an int64 scalar, got tensor with shape: ",
                    key_tensor->shape()));

    // Create copy for insertion into Staging Area
    Tensor key(*key_tensor);

    // Create the tuple to store
    for (std::size_t i = 0; i < values_tensor.size(); ++i) {
      tuple.push_back(values_tensor[i]);
    }

    // Store the tuple in the map
    OP_REQUIRES_OK(ctx, map->put(&key, indices_tensor, &tuple));
  }