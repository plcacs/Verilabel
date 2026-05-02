  void operator()(OpKernelContext* ctx, const Tensor& input,
                  const Tensor& filter, int row_stride, int col_stride,
                  int row_dilation, int col_dilation, const Padding& padding,
                  const std::vector<int64_t>& explicit_paddings, Tensor* output,
                  TensorFormat data_format) {
    DCHECK(data_format == FORMAT_NHWC)
        << "Grouped conv implementation only "
           "supports NHWC tensor format for now.";

    const int64_t in_depth = input.dim_size(3);
    const int64_t patch_depth = filter.dim_size(2);
    const int64_t num_groups = in_depth / patch_depth;

    // Shuffle input/filter tensors to have group as a leading dimension.
    std::array<int64_t, 5> shuffle({3, 0, 1, 2, 4});

    // Compute pre shuffle dimemnsions.
    auto pre_shuffle = [&](const Tensor& tensor) -> std::array<int64, 5> {
      return {tensor.dim_size(0), tensor.dim_size(1), tensor.dim_size(2),
              num_groups, tensor.dim_size(3) / num_groups};
    };

    // Compute post shuffle dimemnsions.
    auto post_shuffle = [&](const Tensor& tensor) -> std::array<int64, 5> {
      return {num_groups, tensor.dim_size(0), tensor.dim_size(1),
              tensor.dim_size(2), tensor.dim_size(3) / num_groups};
    };

    auto& device = ctx->eigen_device<CPUDevice>();

    absl::BlockingCounter shuffles_completed(2);
    auto on_shuffled = [&]() { shuffles_completed.DecrementCount(); };

    // Shuffle input into temporary tensor.
    Tensor input_shuffled;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_temp(input.dtype(), TensorShape(post_shuffle(input)),
                                &input_shuffled));
    input_shuffled.tensor<T, 5>().device(device, on_shuffled) =
        input.shaped<T, 5>(pre_shuffle(input)).shuffle(shuffle);

    // Shuffle filter into temporary tensor.
    Tensor filter_shuffled;
    OP_REQUIRES_OK(ctx, ctx->allocate_temp(filter.dtype(),
                                           TensorShape(post_shuffle(filter)),
                                           &filter_shuffled));
    filter_shuffled.tensor<T, 5>().device(device, on_shuffled) =
        filter.shaped<T, 5>(pre_shuffle(filter)).shuffle(shuffle);

    // Wait for the completion of input/filter shuffles.
    shuffles_completed.Wait();

    // Write group convolution results into temporary output tensor.
    Tensor output_shuffled;
    OP_REQUIRES_OK(ctx, ctx->allocate_temp(output->dtype(),
                                           TensorShape(post_shuffle(*output)),
                                           &output_shuffled));

    for (int64_t i = 0; i < num_groups; ++i) {
      // TODO(ezhulenev): Run this loop using `parallelFor` (regular parallelFor
      // will lead to deadlock, SpatialConvolution has to use async Eigen
      // assignment). This requires small changes to Eigen to support async
      // exeuction for tensor chipping operation.

      // TODO(ezhulenev): Grouped convolution should also support 1x1 filter
      // optimization.

      auto input_slice = input_shuffled.tensor<T, 5>().template chip<0>(i);
      auto filter_slice = filter_shuffled.tensor<T, 5>().template chip<0>(i);
      auto output_slice = output_shuffled.tensor<T, 5>().template chip<0>(i);

      if (padding == EXPLICIT) {
        functor::SpatialConvolution<CPUDevice, T>()(
            ctx->eigen_device<CPUDevice>(), output_slice, input_slice,
            filter_slice, row_stride, col_stride, row_dilation, col_dilation,
            static_cast<int>(explicit_paddings[2]),
            static_cast<int>(explicit_paddings[3]),
            static_cast<int>(explicit_paddings[4]),
            static_cast<int>(explicit_paddings[5]));
      } else {
        functor::SpatialConvolution<CPUDevice, T>()(
            ctx->eigen_device<CPUDevice>(), output_slice, input_slice,
            filter_slice, row_stride, col_stride, row_dilation, col_dilation,
            BrainPadding2EigenPadding(padding));
      }
    }

    // Shuffle temporary output back into pre-shuffled shape.
    std::array<int64_t, 5> rev_shuffle({1, 2, 3, 0, 4});
    output->shaped<T, 5>(pre_shuffle(*output)).device(device) =
        output_shuffled.tensor<T, 5>().shuffle(rev_shuffle);
  }