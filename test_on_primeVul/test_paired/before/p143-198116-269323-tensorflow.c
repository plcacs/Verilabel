  void Compute(OpKernelContext *ctx) override {
    const Tensor *indices_t, *values_t, *shape_t, *reduction_axes_t;
    OP_REQUIRES_OK(ctx, ctx->input("input_indices", &indices_t));
    OP_REQUIRES_OK(ctx, ctx->input("input_values", &values_t));
    OP_REQUIRES_OK(ctx, ctx->input("input_shape", &shape_t));
    OP_REQUIRES_OK(ctx, ctx->input("reduction_axes", &reduction_axes_t));

    OP_REQUIRES_OK(ctx, ValidateInputs(shape_t, reduction_axes_t));

    // TODO(zongheng): we will call Reorder() below, which will modify
    // in-place the underlying indices and values buffers.  To avoid
    // surprises of this kernel being stateful, we work around the above by
    // making deep copies here.  Remove this if/when we change Reorder()'s
    // semantics.
    const auto shape_vec = shape_t->vec<int64>();
    SparseTensor sp;
    OP_REQUIRES_OK(ctx, SparseTensor::Create(
        tensor::DeepCopy(*indices_t), tensor::DeepCopy(*values_t),
                    TensorShape(shape_vec), &sp));
    ReduceDetails reduction = SparseTensorReduceHelper(
        sp, reduction_axes_t->flat<int32>(), keep_dims_);

    Tensor *out_values;
    OP_REQUIRES_OK(
        ctx, ctx->allocate_output(0, reduction.reduced_shape, &out_values));
    auto out_flat = out_values->flat<T>();
    out_flat.setZero();

    Tensor tmp_reduced_val;
    OP_REQUIRES_OK(ctx, ctx->allocate_temp(DataTypeToEnum<T>::value,
                                           TensorShape({}), &tmp_reduced_val));
    auto reduced_val = tmp_reduced_val.scalar<T>();

    // Compute strides, and use it to convert coords to flat index.  The
    // coordinates returned by .group() have the same ndims as group_by_dims.
    gtl::InlinedVector<int64, 8> output_strides(reduction.group_by_dims.size());
    if (!output_strides.empty()) {  // Do this iff we don't reduce all.
      output_strides.back() = 1;
      for (int d = output_strides.size() - 2; d >= 0; --d) {
        output_strides[d] =
            output_strides[d + 1] * shape_vec(reduction.group_by_dims[d + 1]);
      }
    }

    auto CoordinatesToFlatIndex = [](ArraySlice<int64> coords,
                                     ArraySlice<int64> strides) -> int64 {
      if (strides.empty()) {  // Reduce all.
        return 0;
      }
      CHECK_EQ(coords.size(), strides.size());
      int64_t idx = 0;
      for (int i = 0; i < coords.size(); ++i) {
        idx += coords[i] * strides[i];
      }
      return idx;
    };

    // Each group maps one-on-one onto a value in the reduced tensor.
    // g.group() provides the coordinates of a particular reduced value.
    sp.Reorder<T>(reduction.reorder_dims);
    for (const auto &g : sp.group(reduction.group_by_dims)) {
      Op::template Run<T>(ctx, reduced_val, g.template values<T>());
      const int64_t idx = CoordinatesToFlatIndex(g.group(), output_strides);
      out_flat(idx) = reduced_val();
      VLOG(2) << "coords: " << absl::StrJoin(g.group(), ",")
              << "; idx: " << idx << "; group " << Op::Name() << ": "
              << reduced_val();
    }
  }