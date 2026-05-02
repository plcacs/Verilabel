  void Compute(OpKernelContext* ctx) override {
    const Tensor& input = ctx->input(0);
    OP_REQUIRES(
        ctx, (axis_ == -1 || axis_ < input.shape().dims()),
        errors::InvalidArgument("Shape must be at least rank ", axis_ + 1,
                                " but is rank ", input.shape().dims()));
    const int depth = (axis_ == -1) ? 1 : input.dim_size(axis_);
    Tensor input_min_tensor;
    Tensor input_max_tensor;
    Tensor* output = nullptr;
    OP_REQUIRES_OK(ctx, ctx->allocate_output(0, input.shape(), &output));
    if (range_given_) {
      input_min_tensor = ctx->input(1);
      input_max_tensor = ctx->input(2);
      if (axis_ == -1) {
        auto min_val = input_min_tensor.scalar<T>()();
        auto max_val = input_max_tensor.scalar<T>()();
        OP_REQUIRES(ctx, min_val <= max_val,
                    errors::InvalidArgument("Invalid range: input_min ",
                                            min_val, " > input_max ", max_val));
      } else {
        OP_REQUIRES(ctx, input_min_tensor.dim_size(0) == depth,
                    errors::InvalidArgument(
                        "input_min_tensor has incorrect size, was ",
                        input_min_tensor.dim_size(0), " expected ", depth,
                        " to match dim ", axis_, " of the input ",
                        input_min_tensor.shape()));
        OP_REQUIRES(ctx, input_max_tensor.dim_size(0) == depth,
                    errors::InvalidArgument(
                        "input_max_tensor has incorrect size, was ",
                        input_max_tensor.dim_size(0), " expected ", depth,
                        " to match dim ", axis_, " of the input ",
                        input_max_tensor.shape()));
      }
    } else {
      auto range_shape = (axis_ == -1) ? TensorShape({}) : TensorShape({depth});
      OP_REQUIRES_OK(ctx, ctx->allocate_temp(DataTypeToEnum<T>::value,
                                             range_shape, &input_min_tensor));
      OP_REQUIRES_OK(ctx, ctx->allocate_temp(DataTypeToEnum<T>::value,
                                             range_shape, &input_max_tensor));
    }

    if (axis_ == -1) {
      functor::QuantizeAndDequantizeOneScaleFunctor<Device, T> f;
      f(ctx->eigen_device<Device>(), input.flat<T>(), signed_input_, num_bits_,
        range_given_, &input_min_tensor, &input_max_tensor, round_mode_,
        narrow_range_, output->flat<T>());
    } else {
      functor::QuantizeAndDequantizePerChannelFunctor<Device, T> f;
      f(ctx->eigen_device<Device>(),
        input.template flat_inner_outer_dims<T, 3>(axis_ - 1), signed_input_,
        num_bits_, range_given_, &input_min_tensor, &input_max_tensor,
        round_mode_, narrow_range_,
        output->template flat_inner_outer_dims<T, 3>(axis_ - 1));
    }
  }