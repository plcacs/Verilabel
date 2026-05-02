  void Compute(OpKernelContext* ctx) override {
    const Tensor& input = ctx->input(0);

    Tensor* output = nullptr;
    OP_REQUIRES_OK(ctx, ctx->allocate_output(0, input.shape(), &output));

    // One global scale.
    Tensor input_min_tensor(DataTypeToEnum<T>::value, TensorShape());
    Tensor input_max_tensor(DataTypeToEnum<T>::value, TensorShape());
    // Initialize the tensors with the values in the Attrs.
    input_min_tensor.template scalar<T>()() = static_cast<T>(input_min_);
    input_max_tensor.template scalar<T>()() = static_cast<T>(input_max_);

    functor::QuantizeAndDequantizeOneScaleFunctor<Device, T> functor;
    functor(ctx->eigen_device<Device>(), input.flat<T>(), signed_input_,
            num_bits_, range_given_, &input_min_tensor, &input_max_tensor,
            ROUND_HALF_TO_EVEN, /*narrow_range=*/false, output->flat<T>());
  }