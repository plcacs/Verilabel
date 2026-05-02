  void DoRealForwardFFT(OpKernelContext* ctx, uint64* fft_shape,
                        const Tensor& in, Tensor* out) {
    // Create the axes (which are always trailing).
    const auto axes = Eigen::ArrayXi::LinSpaced(FFTRank, 1, FFTRank);
    auto device = ctx->eigen_device<CPUDevice>();
    auto input = Tensor(in).flat_inner_dims<RealT, FFTRank + 1>();
    const auto input_dims = input.dimensions();

    // Slice input to fft_shape on its inner-most dimensions.
    Eigen::DSizes<Eigen::DenseIndex, FFTRank + 1> input_slice_sizes;
    input_slice_sizes[0] = input_dims[0];
    TensorShape temp_shape{input_dims[0]};
    for (int i = 1; i <= FFTRank; ++i) {
      input_slice_sizes[i] = fft_shape[i - 1];
      temp_shape.AddDim(fft_shape[i - 1]);
    }
    OP_REQUIRES(ctx, temp_shape.num_elements() > 0,
                errors::InvalidArgument("Obtained a FFT shape of 0 elements: ",
                                        temp_shape.DebugString()));

    auto output = out->flat_inner_dims<ComplexT, FFTRank + 1>();
    const Eigen::DSizes<Eigen::DenseIndex, FFTRank + 1> zero_start_indices;

    // Compute the full FFT using a temporary tensor.
    Tensor temp;
    OP_REQUIRES_OK(ctx, ctx->allocate_temp(DataTypeToEnum<ComplexT>::v(),
                                           temp_shape, &temp));
    auto full_fft = temp.flat_inner_dims<ComplexT, FFTRank + 1>();
    full_fft.device(device) =
        input.slice(zero_start_indices, input_slice_sizes)
            .template fft<Eigen::BothParts, Eigen::FFT_FORWARD>(axes);

    // Slice away the negative frequency components.
    output.device(device) =
        full_fft.slice(zero_start_indices, output.dimensions());
  }