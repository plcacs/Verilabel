  void DecodePngV2(OpKernelContext* context, StringPiece input) {
    int channel_bits = (data_type_ == DataType::DT_UINT8) ? 8 : 16;
    png::DecodeContext decode;
    OP_REQUIRES(
        context, png::CommonInitDecode(input, channels_, channel_bits, &decode),
        errors::InvalidArgument("Invalid PNG. Failed to initialize decoder."));

    // Verify that width and height are not too large:
    // - verify width and height don't overflow int.
    // - width can later be multiplied by channels_ and sizeof(uint16), so
    //   verify single dimension is not too large.
    // - verify when width and height are multiplied together, there are a few
    //   bits to spare as well.
    const int width = static_cast<int>(decode.width);
    const int height = static_cast<int>(decode.height);
    const int64_t total_size =
        static_cast<int64_t>(width) * static_cast<int64_t>(height);
    if (width != static_cast<int64_t>(decode.width) || width <= 0 ||
        width >= (1LL << 27) || height != static_cast<int64_t>(decode.height) ||
        height <= 0 || height >= (1LL << 27) || total_size >= (1LL << 29)) {
      OP_REQUIRES(context, false,
                  errors::InvalidArgument("PNG size too large for int: ",
                                          decode.width, " by ", decode.height));
    }

    Tensor* output = nullptr;
    Status status;
    // By the existing API, we support decoding PNG with `DecodeGif` op.
    // We need to make sure to return 4-D shapes when using `DecodeGif`.
    if (op_type_ == "DecodeGif") {
      status = context->allocate_output(
          0, TensorShape({1, height, width, decode.channels}), &output);
    } else {
      status = context->allocate_output(
          0, TensorShape({height, width, decode.channels}), &output);
    }

    if (op_type_ == "DecodeBmp") {
      // TODO(b/171060723): Only DecodeBmp as op_type_ is not acceptable here
      // because currently `decode_(jpeg|png|gif)` ops can decode any one of
      // jpeg, png or gif but not bmp. Similarly, `decode_bmp` cannot decode
      // anything but bmp formats. This behavior needs to be revisited. For more
      // details, please refer to the bug.
      OP_REQUIRES(context, false,
                  errors::InvalidArgument(
                      "Trying to decode PNG format using DecodeBmp op. Use "
                      "`decode_png` or `decode_image` instead."));
    } else if (op_type_ == "DecodeAndCropJpeg") {
      OP_REQUIRES(context, false,
                  errors::InvalidArgument(
                      "DecodeAndCropJpeg operation can run on JPEG only, but "
                      "detected PNG."));
    }

    if (!status.ok()) png::CommonFreeDecode(&decode);
    OP_REQUIRES_OK(context, status);

    if (data_type_ == DataType::DT_UINT8) {
      OP_REQUIRES(
          context,
          png::CommonFinishDecode(
              reinterpret_cast<png_bytep>(output->flat<uint8>().data()),
              decode.channels * width * sizeof(uint8), &decode),
          errors::InvalidArgument("Invalid PNG data, size ", input.size()));
    } else if (data_type_ == DataType::DT_UINT16) {
      OP_REQUIRES(
          context,
          png::CommonFinishDecode(
              reinterpret_cast<png_bytep>(output->flat<uint16>().data()),
              decode.channels * width * sizeof(uint16), &decode),
          errors::InvalidArgument("Invalid PNG data, size ", input.size()));
    } else if (data_type_ == DataType::DT_FLOAT) {
      // `png::CommonFinishDecode` does not support `float`. First allocate
      // uint16 buffer for the image and decode in uint16 (lossless). Wrap the
      // buffer in `unique_ptr` so that we don't forget to delete the buffer.
      std::unique_ptr<uint16[]> buffer(
          new uint16[height * width * decode.channels]);
      OP_REQUIRES(
          context,
          png::CommonFinishDecode(reinterpret_cast<png_bytep>(buffer.get()),
                                  decode.channels * width * sizeof(uint16),
                                  &decode),
          errors::InvalidArgument("Invalid PNG data, size ", input.size()));

      // Convert uint16 image data to desired data type.
      // Use eigen threadpooling to speed up the copy operation.
      const auto& device = context->eigen_device<Eigen::ThreadPoolDevice>();
      TTypes<uint16, 3>::UnalignedConstTensor buf(buffer.get(), height, width,
                                                  decode.channels);
      float scale = 1. / std::numeric_limits<uint16>::max();
      // Fill output tensor with desired dtype.
      output->tensor<float, 3>().device(device) = buf.cast<float>() * scale;
    }
  }