  void Compute(OpKernelContext* context) override {
    const Tensor& image = context->input(0);
    OP_REQUIRES(context, image.dims() == 3,
                errors::InvalidArgument("image must be 3-dimensional",
                                        image.shape().DebugString()));
    OP_REQUIRES(
        context,
        FastBoundsCheck(image.NumElements(), std::numeric_limits<int32>::max()),
        errors::InvalidArgument("image cannot have >= int32 max elements"));
    const int32 height = static_cast<int32>(image.dim_size(0));
    const int32 width = static_cast<int32>(image.dim_size(1));
    const int32 channels = static_cast<int32>(image.dim_size(2));

    // In some cases, we pass width*channels*2 to png.
    const int32 max_row_width = std::numeric_limits<int32>::max() / 2;

    OP_REQUIRES(context, FastBoundsCheck(width * channels, max_row_width),
                errors::InvalidArgument("image too wide to encode"));

    OP_REQUIRES(context, channels >= 1 && channels <= 4,
                errors::InvalidArgument(
                    "image must have 1, 2, 3, or 4 channels, got ", channels));

    // Encode image to png string
    Tensor* output = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(0, TensorShape({}), &output));
    if (desired_channel_bits_ == 8) {
      OP_REQUIRES(context,
                  png::WriteImageToBuffer(
                      image.flat<uint8>().data(), width, height,
                      width * channels, channels, desired_channel_bits_,
                      compression_, &output->scalar<tstring>()(), nullptr),
                  errors::Internal("PNG encoding failed"));
    } else {
      OP_REQUIRES(context,
                  png::WriteImageToBuffer(
                      image.flat<uint16>().data(), width, height,
                      width * channels * 2, channels, desired_channel_bits_,
                      compression_, &output->scalar<tstring>()(), nullptr),
                  errors::Internal("PNG encoding failed"));
    }
  }