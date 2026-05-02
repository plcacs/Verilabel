  void Compute(OpKernelContext* ctx) override {
    const Tensor* x_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("x", &x_tensor));

    const Tensor* cs_prev_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("cs_prev", &cs_prev_tensor));

    const Tensor* h_prev_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("h_prev", &h_prev_tensor));

    const Tensor* w_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("w", &w_tensor));

    const Tensor* wci_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("wci", &wci_tensor));

    const Tensor* wcf_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("wcf", &wcf_tensor));

    const Tensor* wco_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("wco", &wco_tensor));

    const Tensor* b_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->input("b", &b_tensor));

    const int64_t batch_size = x_tensor->dim_size(0);
    const int64_t input_size = x_tensor->dim_size(1);
    const int64_t cell_size = cs_prev_tensor->dim_size(1);

    // Sanity checks for our input shapes.
    OP_REQUIRES(ctx, cs_prev_tensor->dim_size(0) == batch_size,
                errors::InvalidArgument("cs_prev.dims(0) != batch_size: ",
                                        cs_prev_tensor->dim_size(0), " vs. ",
                                        batch_size));
    OP_REQUIRES(ctx, cs_prev_tensor->dim_size(1) == cell_size,
                errors::InvalidArgument("cs_prev.dims(1) != cell_size: ",
                                        cs_prev_tensor->dim_size(1), " vs. ",
                                        cell_size));

    OP_REQUIRES(ctx, h_prev_tensor->dim_size(0) == batch_size,
                errors::InvalidArgument("h_prev.dims(0) != batch_size: ",
                                        h_prev_tensor->dim_size(0), " vs. ",
                                        batch_size));
    OP_REQUIRES(ctx, h_prev_tensor->dim_size(1) == cell_size,
                errors::InvalidArgument(
                    "h_prev.dims(1) != cell_size: ", h_prev_tensor->dim_size(1),
                    " vs. ", cell_size));

    OP_REQUIRES(ctx, w_tensor->dim_size(0) == input_size + cell_size,
                errors::InvalidArgument(
                    "w.dim_size(0) != input_size + cell_size: ",
                    w_tensor->dim_size(0), " vs. ", input_size + cell_size));
    OP_REQUIRES(ctx, w_tensor->dim_size(1) == cell_size * 4,
                errors::InvalidArgument(
                    "w.dim_size(1) != cell_size * 4: ", w_tensor->dim_size(1),
                    " vs. ", cell_size * 4));

    OP_REQUIRES(ctx, b_tensor->dim_size(0) == cell_size * 4,
                errors::InvalidArgument(
                    "b.dim_size(0) != cell_size * 4: ", b_tensor->dim_size(0),
                    " vs. ", cell_size * 4));

    // Allocate our output tensors.
    Tensor* i_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->forward_input_or_allocate_output(
                            {"h_prev"}, "i",
                            TensorShape({batch_size, cell_size}), &i_tensor));

    Tensor* cs_tensor = nullptr;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output("cs", TensorShape({batch_size, cell_size}),
                                  &cs_tensor));

    Tensor* f_tensor = nullptr;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output("f", TensorShape({batch_size, cell_size}),
                                  &f_tensor));

    Tensor* o_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->forward_input_or_allocate_output(
                            {"cs_prev"}, "o",
                            TensorShape({batch_size, cell_size}), &o_tensor));

    Tensor* ci_tensor = nullptr;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output("ci", TensorShape({batch_size, cell_size}),
                                  &ci_tensor));

    Tensor* co_tensor = nullptr;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output("co", TensorShape({batch_size, cell_size}),
                                  &co_tensor));

    Tensor* h_tensor = nullptr;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output("h", TensorShape({batch_size, cell_size}),
                                  &h_tensor));

    // Allocate our temp tensors.
    Tensor xh_tensor;
    OP_REQUIRES_OK(ctx, ctx->allocate_temp(
                            DataTypeToEnum<T>::v(),
                            TensorShape({batch_size, input_size + cell_size}),
                            &xh_tensor));

    Tensor gates_tensor;
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_temp(DataTypeToEnum<T>::v(),
                                      TensorShape({batch_size, cell_size * 4}),
                                      &gates_tensor));

    const Device& device = ctx->eigen_device<Device>();

    functor::LSTMBlockCellFprop<Device, T, USE_CUBLAS, gate_layout>(
        batch_size, input_size, cell_size)(
        ctx, device, forget_bias_, cell_clip_, use_peephole_,
        x_tensor->matrix<T>(), cs_prev_tensor->matrix<T>(),
        h_prev_tensor->matrix<T>(), w_tensor->matrix<T>(), wci_tensor->vec<T>(),
        wcf_tensor->vec<T>(), wco_tensor->vec<T>(), b_tensor->vec<T>(),
        xh_tensor.matrix<T>(), i_tensor->matrix<T>(), cs_tensor->matrix<T>(),
        f_tensor->matrix<T>(), o_tensor->matrix<T>(), ci_tensor->matrix<T>(),
        co_tensor->matrix<T>(), gates_tensor.matrix<T>(),
        h_tensor->matrix<T>());
  }