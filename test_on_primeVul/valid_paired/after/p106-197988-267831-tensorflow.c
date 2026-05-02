  void Compute(OpKernelContext* context) override {
    const Tensor* reverse_index_map_t;
    const Tensor* grad_values_t;
    OP_REQUIRES_OK(context,
                   context->input("reverse_index_map", &reverse_index_map_t));
    OP_REQUIRES_OK(context, context->input("grad_values", &grad_values_t));

    const CPUDevice& d = context->eigen_device<CPUDevice>();

    OP_REQUIRES(
        context, TensorShapeUtils::IsVector(reverse_index_map_t->shape()),
        errors::InvalidArgument("reverse_index_map must be a vector, saw: ",
                                reverse_index_map_t->shape().DebugString()));
    OP_REQUIRES(context, TensorShapeUtils::IsVector(grad_values_t->shape()),
                errors::InvalidArgument("grad_values must be a vector, saw: ",
                                        grad_values_t->shape().DebugString()));

    const auto reverse_index_map = reverse_index_map_t->vec<int64>();
    const auto grad_values = grad_values_t->vec<T>();

    const int64 N = reverse_index_map_t->shape().dim_size(0);
    const int64 N_full = grad_values_t->shape().dim_size(0);

    Tensor* d_values_t;
    OP_REQUIRES_OK(context, context->allocate_output(
                                "d_values", TensorShape({N}), &d_values_t));
    auto d_values = d_values_t->vec<T>();
    Tensor* d_default_value_t;
    OP_REQUIRES_OK(context,
                   context->allocate_output("d_default_value", TensorShape({}),
                                            &d_default_value_t));
    T& d_default_value = d_default_value_t->scalar<T>()();
    d_default_value = T();

    Tensor visited_t;
    OP_REQUIRES_OK(context, context->allocate_temp(
                                DT_BOOL, TensorShape({N_full}), &visited_t));
    auto visited = visited_t.vec<bool>();
    visited.device(d) = visited.constant(false);

    for (int i = 0; i < N; ++i) {
      // Locate the index of the output of the forward prop associated
      // with this location in the input of the forward prop.  Copy
      // the gradient into it.  Mark it as visited.
      int64 reverse_index = reverse_index_map(i);
      OP_REQUIRES(
          context, 0 <= reverse_index && reverse_index < N_full,
          errors::InvalidArgument("Elements in reverse index must be in [0, ",
                                  N_full, ") but got ", reverse_index));
      d_values(i) = grad_values(reverse_index);
      visited(reverse_index) = true;
    }
    for (int j = 0; j < N_full; ++j) {
      // The default value gradient gets the accumulated remainder of
      // the backprop values (since the default value was used to fill
      // in these slots in the forward calculation).
      if (!visited(j)) {
        d_default_value += grad_values(j);
      }
    }
  }