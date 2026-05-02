  void Compute(OpKernelContext* ctx) override {
    const auto splits = ctx->input(0).flat<int64>();
    const auto values = ctx->input(1).flat<Tidx>();
    const Tensor& size_t = ctx->input(2);
    const auto weights = ctx->input(3).flat<T>();
    const int64 weights_size = weights.size();

    Tidx size = size_t.scalar<Tidx>()();
    OP_REQUIRES(
        ctx, size >= 0,
        errors::InvalidArgument("size (", size, ") must be non-negative"));

    int num_rows = splits.size() - 1;
    int num_values = values.size();
    int batch_idx = 0;

    Tensor* out_t;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output(0, TensorShape({num_rows, size}), &out_t));
    functor::SetZeroFunctor<Device, T> fill;
    fill(ctx->eigen_device<Device>(), out_t->flat<T>());
    const auto out = out_t->matrix<T>();

    for (int idx = 0; idx < num_values; ++idx) {
      while (idx >= splits(batch_idx)) {
        batch_idx++;
      }
      Tidx bin = values(idx);
      OP_REQUIRES(ctx, bin >= 0,
                  errors::InvalidArgument("Input must be non-negative"));
      if (bin < size) {
        if (binary_output_) {
          out(batch_idx - 1, bin) = T(1);
        } else {
          T value = (weights_size > 0) ? weights(idx) : T(1);
          out(batch_idx - 1, bin) += value;
        }
      }
    }
  }