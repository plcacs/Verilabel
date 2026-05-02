  explicit ReverseSequenceOp(OpKernelConstruction* context)
      : OpKernel(context) {
    OP_REQUIRES_OK(context, context->GetAttr("batch_dim", &batch_dim_));
    OP_REQUIRES_OK(context, context->GetAttr("seq_dim", &seq_dim_));
    OP_REQUIRES(context, batch_dim_ >= 0,
                errors::InvalidArgument("Invalid batch_dim ", batch_dim_));
    OP_REQUIRES(context, seq_dim_ >= 0,
                errors::InvalidArgument("Invalid seq_dim ", seq_dim_));
  }