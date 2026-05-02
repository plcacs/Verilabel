  void Compute(OpKernelContext* ctx) override {
    const Tensor& input = ctx->input(0);
    const Tensor& input_min_tensor = ctx->input(1);
    const Tensor& input_max_tensor = ctx->input(2);

    int num_slices = 1;
    if (axis_ > -1) {
      num_slices = input.dim_size(axis_);
    }
    OP_REQUIRES(ctx, input_min_tensor.NumElements() == num_slices,
                errors::InvalidArgument(
                    "input_min_tensor must have as many elements as input on "
                    "the dequantization axis (",
                    axis_, "), got ", input_min_tensor.NumElements(),
                    ", expected ", num_slices));
    OP_REQUIRES(ctx, input_max_tensor.NumElements() == num_slices,
                errors::InvalidArgument(
                    "input_max_tensor must have as many elements as input on "
                    "the dequantization axis (",
                    axis_, "), got ", input_max_tensor.NumElements(),
                    ", expected ", num_slices));

    Tensor* output = nullptr;
    OP_REQUIRES_OK(ctx, ctx->allocate_output(0, input.shape(), &output));
    Tensor float_output =
        need_cast_ ? tensorflow::Tensor(DT_FLOAT, input.shape()) : *output;
    if (num_slices == 1) {
      const float min_range = input_min_tensor.flat<float>()(0);
      const float max_range = input_max_tensor.flat<float>()(0);
      DequantizeTensor(ctx, input, min_range, max_range, &float_output);
    } else {
      OP_REQUIRES(ctx, mode_ != QUANTIZE_MODE_MIN_FIRST,
                  errors::Unimplemented("MIN_FIRST mode is not implemented for "
                                        "Dequantize with axis != -1."));

      int64_t pre_dim = 1, post_dim = 1;
      for (int i = 0; i < axis_; ++i) {
        pre_dim *= float_output.dim_size(i);
      }
      for (int i = axis_ + 1; i < float_output.dims(); ++i) {
        post_dim *= float_output.dim_size(i);
      }
      auto input_tensor = input.template bit_casted_shaped<T, 3>(
          {pre_dim, num_slices, post_dim});
      auto output_tensor =
          float_output.flat_inner_outer_dims<float, 3>(axis_ - 1);
      auto min_ranges = input_min_tensor.vec<float>();
      auto max_ranges = input_max_tensor.vec<float>();
      for (int i = 0; i < num_slices; ++i) {
        DequantizeSlice(ctx->eigen_device<Device>(), ctx,
                        input_tensor.template chip<1>(i), min_ranges(i),
                        max_ranges(i), output_tensor.template chip<1>(i));
      }
    }
    if (need_cast_) {
      S* out_ptr = output->flat<S>().data();
      float* in_ptr = float_output.flat<float>().data();
      for (int64_t i = 0; i < float_output.NumElements(); ++i) {
        out_ptr[i] = static_cast<S>(in_ptr[i]);
      }
    }
  }