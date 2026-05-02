  void Compute(OpKernelContext* context) override {
    const Tensor& logits_in = context->input(0);
    const Tensor& labels_in = context->input(1);

    TensorShape shape_in = logits_in.shape();

    BCast bcast(BCast::FromShape(logits_in.shape()),
                BCast::FromShape(labels_in.shape()));
    if (!logits_in.IsSameSize(labels_in)) {
      OP_REQUIRES(context, bcast.IsValid(),
                  errors::InvalidArgument(
                      "logits and labels must be broadcastable: logits_size=",
                      logits_in.shape().DebugString(),
                      " labels_size=", labels_in.shape().DebugString()));
      shape_in = BCast::ToShape(bcast.output_shape());
    }
    OP_REQUIRES(context, TensorShapeUtils::IsMatrix(shape_in),
                errors::InvalidArgument("logits and labels must be either "
                                        "2-dimensional, or broadcasted to be "
                                        "2-dimensional"));

    if (std::is_same<Device, GPUDevice>::value) {
      OP_REQUIRES(context, !OpDeterminismRequired(),
                  errors::Unimplemented(
                      "The GPU implementation of SoftmaxCrossEntropyWithLogits"
                      " that would have been executed is not deterministic."
                      " Note that the Python API uses an alternative,"
                      " deterministic, GPU-accelerated path when determinism is"
                      " enabled."));
    }

    // loss is 1-D (one per example), and size is batch_size.

    Tensor scratch;
    OP_REQUIRES_OK(
        context, context->allocate_temp(DataTypeToEnum<T>::value,
                                        TensorShape({shape_in.dim_size(0), 1}),
                                        &scratch));

    Tensor* loss_out = nullptr;
    OP_REQUIRES_OK(context,
                   context->allocate_output(
                       0, TensorShape({shape_in.dim_size(0)}), &loss_out));
    Tensor* back_out = nullptr;
    // Try to reuse the logits_in buffer for the backprop output.
    OP_REQUIRES_OK(context, context->forward_input_or_allocate_output(
                                {0}, 1, shape_in, &back_out));
    if (shape_in.dim_size(0) > 0) {
      functor::XentFunctor<Device, T> functor;
      if (logits_in.IsSameSize(labels_in)) {
        functor(context->eigen_device<Device>(), shape_in.AsEigenDSizes<2>(),
                Eigen::array<Eigen::DenseIndex, 2>{1, 1},
                Eigen::array<Eigen::DenseIndex, 2>{1, 1}, logits_in.matrix<T>(),
                labels_in.matrix<T>(), scratch.matrix<T>(), loss_out->vec<T>(),
                back_out->matrix<T>());
      } else {
        functor(context->eigen_device<Device>(), shape_in.AsEigenDSizes<2>(),
                BCast::ToIndexArray<2>(bcast.x_bcast()),
                BCast::ToIndexArray<2>(bcast.y_bcast()),
                logits_in.template shaped<T, 2>(bcast.x_reshape()),
                labels_in.template shaped<T, 2>(bcast.y_reshape()),
                scratch.matrix<T>(), loss_out->vec<T>(), back_out->matrix<T>());
      }
    }
  }