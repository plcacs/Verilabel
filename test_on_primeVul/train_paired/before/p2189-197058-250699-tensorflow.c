  void Compute(OpKernelContext* context) override {
    const Tensor& contents = context->input(0);
    OP_REQUIRES(context, TensorShapeUtils::IsScalar(contents.shape()),
                errors::InvalidArgument("contents must be scalar, got shape ",
                                        contents.shape().DebugString()));

    // Start decoding image to get shape details
    const StringPiece input = contents.scalar<string>()();

    OP_REQUIRES(context, (32 <= input.size()),
                errors::InvalidArgument("Incomplete bmp content, requires at "
                                        "least 32 bytes to find the header "
                                        "size, width, height, and bpp, got ",
                                        input.size(), " bytes"));

    const uint8* img_bytes = reinterpret_cast<const uint8*>(input.data());
    int32 header_size_ = internal::SubtleMustCopy(
        *(reinterpret_cast<const int32*>(img_bytes + 10)));
    const int32 header_size = ByteSwapInt32ForBigEndian(header_size_);
    int32 width_ = internal::SubtleMustCopy(
        *(reinterpret_cast<const int32*>(img_bytes + 18)));
    const int32 width = ByteSwapInt32ForBigEndian(width_);
    int32 height_ = internal::SubtleMustCopy(
        *(reinterpret_cast<const int32*>(img_bytes + 22)));
    const int32 height = ByteSwapInt32ForBigEndian(height_);
    int32 bpp_ = internal::SubtleMustCopy(
        *(reinterpret_cast<const int32*>(img_bytes + 28)));
    const int32 bpp = ByteSwapInt32ForBigEndian(bpp_);

    if (channels_) {
      OP_REQUIRES(context, (channels_ == bpp / 8),
                  errors::InvalidArgument(
                      "channels attribute ", channels_,
                      " does not match bits per pixel from file ", bpp / 8));
    } else {
      channels_ = bpp / 8;
    }

    // Current implementation only supports 1, 3 or 4 channel
    // bitmaps.
    OP_REQUIRES(context, (channels_ == 1 || channels_ == 3 || channels_ == 4),
                errors::InvalidArgument(
                    "Number of channels must be 1, 3 or 4, was ", channels_));

    // there may be padding bytes when the width is not a multiple of 4 bytes
    // 8 * channels == bits per pixel
    const int row_size = (8 * channels_ * width + 31) / 32 * 4;

    const int last_pixel_offset =
        header_size + (abs(height) - 1) * row_size + (width - 1) * channels_;

    // [expected file size] = [last pixel offset] + [last pixel size=channels]
    const int expected_file_size = last_pixel_offset + channels_;

    OP_REQUIRES(
        context, (expected_file_size <= input.size()),
        errors::InvalidArgument("Incomplete bmp content, requires at least ",
                                expected_file_size, " bytes, got ",
                                input.size(), " bytes"));

    // if height is negative, data layout is top down
    // otherwise, it's bottom up
    bool top_down = (height < 0);

    // Decode image, allocating tensor once the image size is known
    Tensor* output = nullptr;
    OP_REQUIRES_OK(
        context, context->allocate_output(
                     0, TensorShape({abs(height), width, channels_}), &output));

    const uint8* bmp_pixels = &img_bytes[header_size];

    Decode(bmp_pixels, row_size, output->flat<uint8>().data(), width,
           abs(height), channels_, top_down);
  }