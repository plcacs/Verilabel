  void Compute(OpKernelContext* context) override {
    const Tensor& input_indices_in = context->input(0);
    const Tensor& input_shape_in = context->input(1);

    OP_REQUIRES(context, TensorShapeUtils::IsMatrix(input_indices_in.shape()),
                errors::InvalidArgument("Input must be a matrix."));
    OP_REQUIRES(context, TensorShapeUtils::IsVector(input_shape_in.shape()),
                errors::InvalidArgument("Input shape must be a vector."));
    OP_REQUIRES(context,
                input_indices_in.dim_size(1) == input_shape_in.dim_size(0),
                errors::InvalidArgument(
                    "Input tensor rank must match input shape length."));
    ReshapeSparseTensor<Device>(context, context->input(0), context->input(1),
                                context->input(2), 0 /* output indices index */,
                                1 /* output shape index */);
  }