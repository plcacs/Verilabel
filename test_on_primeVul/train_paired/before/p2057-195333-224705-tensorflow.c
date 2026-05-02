  void Compute(OpKernelContext* context) final {
    const Tensor& superdiag = context->input(0);
    const Tensor& maindiag = context->input(1);
    const Tensor& subdiag = context->input(2);
    const Tensor& rhs = context->input(3);

    const int ndims = rhs.dims();
    int64 batch_size = 1;
    for (int i = 0; i < ndims - 2; i++) {
      batch_size *= rhs.dim_size(i);
    }
    const int m = rhs.dim_size(ndims - 2);
    const int n = rhs.dim_size(ndims - 1);

    // Allocate output.
    Tensor* output;
    OP_REQUIRES_OK(context, context->allocate_output(0, rhs.shape(), &output));

    const Eigen::GpuDevice& device = context->eigen_device<Eigen::GpuDevice>();
    GpuLaunchConfig cfg = GetGpuLaunchConfig(1, device);
    TF_CHECK_OK(GpuLaunchKernel(
        TridiagonalMatMulKernel<Scalar>, cfg.block_count, cfg.thread_per_block,
        0, device.stream(), batch_size, m, n, superdiag.flat<Scalar>().data(),
        maindiag.flat<Scalar>().data(), subdiag.flat<Scalar>().data(),
        rhs.flat<Scalar>().data(), output->flat<Scalar>().data()));
  }