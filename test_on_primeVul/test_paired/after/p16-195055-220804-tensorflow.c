  void Compute(OpKernelContext* context) override {
    const Tensor& splits = context->input(0);
    const Tensor& values = context->input(1);
    const Tensor& weights = context->input(2);
    bool use_weights = weights.NumElements() > 0;
    bool is_1d = false;

    if (use_weights) {
      OP_REQUIRES(
          context, weights.shape() == values.shape(),
          errors::InvalidArgument(
              "Weights and values must have the same shape. Weight shape: ",
              weights.shape().DebugString(),
              "; values shape: ", values.shape().DebugString()));
    }

    const auto splits_values = splits.flat<int64_t>();
    const auto values_values = values.flat<T>();
    const auto weight_values = weights.flat<W>();
    int num_batches = splits.NumElements() - 1;
    int num_values = values.NumElements();

    OP_REQUIRES(
        context, num_batches > 0,
        errors::InvalidArgument(
            "Must provide at least 2 elements for the splits argument"));
    OP_REQUIRES(context, splits_values(0) == 0,
                errors::InvalidArgument("Splits must start with 0, not with ",
                                        splits_values(0)));
    OP_REQUIRES(context, splits_values(num_batches) == num_values,
                errors::InvalidArgument(
                    "Splits must end with the number of values, got ",
                    splits_values(num_batches), " instead of ", num_values));

    auto per_batch_counts = BatchedMap<W>(num_batches);
    T max_value = 0;
    int batch_idx = 0;

    for (int idx = 0; idx < num_values; ++idx) {
      while (idx >= splits_values(batch_idx)) {
        batch_idx++;
      }
      const auto& value = values_values(idx);
      if (value >= 0 && (maxlength_ <= 0 || value < maxlength_)) {
        if (binary_output_) {
          per_batch_counts[batch_idx - 1][value] = 1;
        } else if (use_weights) {
          per_batch_counts[batch_idx - 1][value] += weight_values(idx);
        } else {
          per_batch_counts[batch_idx - 1][value]++;
        }
        if (value > max_value) {
          max_value = value;
        }
      }
    }

    int num_output_values = GetOutputSize(max_value, maxlength_, minlength_);
    OP_REQUIRES_OK(context, OutputSparse<W>(per_batch_counts, num_output_values,
                                            is_1d, context));
  }