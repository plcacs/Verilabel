  void Compute(OpKernelContext* ctx) override {
    const Tensor& input = ctx->input(0);
    const Tensor& input_min_range = ctx->input(1);
    const Tensor& input_max_range = ctx->input(2);

    int num_slices = 1;
    if (axis_ > -1) {
      OP_REQUIRES(
          ctx, input.dims() > axis_,
          errors::InvalidArgument(
              "Axis is on a zero-based index, so its value must always be less "
              "than number of input's dims, but given axis value was ",
              axis_, " and input's dims was ", input.dims()));
      num_slices = input.dim_size(axis_);
      OP_REQUIRES(ctx, input_min_range.dims() == 1,
                  errors::InvalidArgument(
                      "If axis is specified, min_range must be a 1-D tensor "
                      "whose size matches the axis dimension of the input and "
                      "output tensors, but min_range dims are ",
                      input_min_range.dims()));
      OP_REQUIRES(ctx, input_min_range.dim_size(0) == num_slices,
                  errors::InvalidArgument(
                      "If axis is specified, min_range must be a 1-D tensor "
                      "whose size matches the axis dimension of the input and "
                      "output tensors, but min_range is a 1-D tensor of size ",
                      input_min_range.dim_size(0),
                      " and input's axis dimension is of size ", num_slices));
      OP_REQUIRES(ctx, input_max_range.dims() == 1,
                  errors::InvalidArgument(
                      "If axis is specified, max_range must be a 1-D tensor "
                      "whose size matches the axis dimension of the input and "
                      "output tensors, but max_range dims are ",
                      input_max_range.dims()));
      OP_REQUIRES(ctx, input_max_range.dim_size(0) == num_slices,
                  errors::InvalidArgument(
                      "If axis is specified, max_range must be a 1-D tensor "
                      "whose size matches the axis dimension of the input and "
                      "output tensors, but max_range is a 1-D tensor of size ",
                      input_max_range.dim_size(0),
                      " and input's axis dimension is of size ", num_slices));
    } else {
      OP_REQUIRES(ctx, input_min_range.NumElements() == 1,
                  errors::InvalidArgument(
                      "If axis is not specified, min_range must contain a "
                      "single float element, but it contains ",
                      input_min_range.NumElements(), " elements"));
      OP_REQUIRES(ctx, input_max_range.NumElements() == 1,
                  errors::InvalidArgument(
                      "If axis is not specified, max_range must contain a "
                      "single float element, but it contains ",
                      input_max_range.NumElements(), " elements"));
    }

    const TensorShape& minmax_shape = ctx->input(1).shape();
    Tensor* output = nullptr;
    OP_REQUIRES_OK(ctx, ctx->allocate_output(0, input.shape(), &output));

    Tensor* output_min_tensor = nullptr;
    Tensor* output_max_tensor = nullptr;

    if (num_slices == 1) {
      OP_REQUIRES_OK(ctx, ctx->allocate_output(1, {}, &output_min_tensor));
      OP_REQUIRES_OK(ctx, ctx->allocate_output(2, {}, &output_max_tensor));
      const float min_range = input_min_range.template flat<float>()(0);
      const float max_range = input_max_range.template flat<float>()(0);
      QuantizeTensor(ctx, input, min_range, max_range, output,
                     output_min_tensor, output_max_tensor);
      return;
    }

    OP_REQUIRES(ctx, mode_ != QUANTIZE_MODE_MIN_FIRST,
                errors::Unimplemented("MIN_FIRST mode is not implemented for "
                                      "Quantize with axis != -1."));
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_output(1, minmax_shape, &output_min_tensor));
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_output(2, minmax_shape, &output_max_tensor));

    auto input_tensor =
        input.template flat_inner_outer_dims<float, 3>(axis_ - 1);
    int64_t pre_dim = 1, post_dim = 1;
    for (int i = 0; i < axis_; ++i) {
      pre_dim *= output->dim_size(i);
    }
    for (int i = axis_ + 1; i < output->dims(); ++i) {
      post_dim *= output->dim_size(i);
    }
    auto output_tensor = output->template bit_casted_shaped<T, 3>(
        {pre_dim, num_slices, post_dim});
    auto min_ranges = input_min_range.template vec<float>();
    auto max_ranges = input_max_range.template vec<float>();
    for (int i = 0; i < num_slices; ++i) {
      QuantizeSlice(ctx->eigen_device<Device>(), ctx,
                    input_tensor.template chip<1>(i), min_ranges(i),
                    max_ranges(i), output_tensor.template chip<1>(i),
                    &output_min_tensor->flat<float>()(i),
                    &output_max_tensor->flat<float>()(i));
    }
  }