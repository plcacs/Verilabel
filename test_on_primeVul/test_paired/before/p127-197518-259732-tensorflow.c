  void Compute(OpKernelContext* ctx) override {
    const Tensor& gradient = ctx->input(0);
    const Tensor& input = ctx->input(1);
    Tensor* input_backprop = nullptr;
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_output(0, input.shape(), &input_backprop));
    OP_REQUIRES(
        ctx, axis_ >= -1,
        errors::InvalidArgument("Axis must be at least -1. Found ", axis_));
    OP_REQUIRES(ctx, (axis_ == -1 || axis_ < input.shape().dims()),
                errors::InvalidArgument(
                    "Axis should be -1 or 0 or a positive value less than ",
                    input.shape().dims(), "but given axis value was ", axis_));

    OP_REQUIRES(
        ctx, input.IsSameSize(gradient),
        errors::InvalidArgument("gradient and input must be the same size"));
    const int depth = (axis_ == -1) ? 1 : input.dim_size(axis_);
    const Tensor& input_min_tensor = ctx->input(2);
    OP_REQUIRES(ctx,
                input_min_tensor.dims() == 0 || input_min_tensor.dims() == 1,
                errors::InvalidArgument(
                    "Input min tensor must have dimension 1. Recieved ",
                    input_min_tensor.dims(), "."));
    const Tensor& input_max_tensor = ctx->input(3);
    OP_REQUIRES(ctx,
                input_max_tensor.dims() == 0 || input_max_tensor.dims() == 1,
                errors::InvalidArgument(
                    "Input max tensor must have dimension 1. Recieved ",
                    input_max_tensor.dims(), "."));
    if (axis_ != -1) {
      OP_REQUIRES(
          ctx, input_min_tensor.dim_size(0) == depth,
          errors::InvalidArgument("min has incorrect size, expected ", depth,
                                  " was ", input_min_tensor.dim_size(0)));
      OP_REQUIRES(
          ctx, input_max_tensor.dim_size(0) == depth,
          errors::InvalidArgument("max has incorrect size, expected ", depth,
                                  " was ", input_max_tensor.dim_size(0)));
    }

    TensorShape min_max_shape(input_min_tensor.shape());
    Tensor* input_min_backprop;
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_output(1, min_max_shape, &input_min_backprop));

    Tensor* input_max_backprop;
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_output(2, min_max_shape, &input_max_backprop));

    if (axis_ == -1) {
      functor::QuantizeAndDequantizeOneScaleGradientFunctor<Device, T> f;
      f(ctx->eigen_device<Device>(), gradient.template flat<T>(),
        input.template flat<T>(), input_min_tensor.scalar<T>(),
        input_max_tensor.scalar<T>(), input_backprop->template flat<T>(),
        input_min_backprop->template scalar<T>(),
        input_max_backprop->template scalar<T>());
    } else {
      functor::QuantizeAndDequantizePerChannelGradientFunctor<Device, T> f;
      f(ctx->eigen_device<Device>(),
        gradient.template flat_inner_outer_dims<T, 3>(axis_ - 1),
        input.template flat_inner_outer_dims<T, 3>(axis_ - 1),
        &input_min_tensor, &input_max_tensor,
        input_backprop->template flat_inner_outer_dims<T, 3>(axis_ - 1),
        input_min_backprop->template flat<T>(),
        input_max_backprop->template flat<T>());
    }
  }