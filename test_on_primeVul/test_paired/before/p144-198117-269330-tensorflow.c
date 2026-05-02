  void Compute(OpKernelContext* context) override {
    // Here's the basic idea:
    // Batch and depth dimension are independent from row and col dimension. And
    // because FractionalAvgPool currently only support pooling along row and
    // col, we can basically think of this 4D tensor backpropagation as
    // operation of a series of 2D planes.
    //
    // For each element of a 'slice' (2D plane) of output_backprop, we need to
    // figure out its contributors when doing FractionalAvgPool operation. This
    // can be done based on row_pooling_sequence, col_pooling_seq and
    // overlapping.
    // Once we figure out the original contributors, we just need to evenly
    // divide the value of this element among these contributors.
    //
    // Internally, we divide the out_backprop tensor and store it in a temporary
    // tensor of double type. And cast it to the corresponding type.
    typedef Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>>
        ConstEigenMatrixMap;
    typedef Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>>
        EigenDoubleMatrixMap;

    // Grab the inputs.
    const Tensor& orig_input_tensor_shape = context->input(0);
    OP_REQUIRES(context,
                orig_input_tensor_shape.dims() == 1 &&
                    orig_input_tensor_shape.NumElements() == 4,
                errors::InvalidArgument("original input tensor shape must be"
                                        "1-dimensional and 4 elements"));
    const Tensor& out_backprop = context->input(1);
    const Tensor& row_seq_tensor = context->input(2);
    const Tensor& col_seq_tensor = context->input(3);

    const int64_t out_batch = out_backprop.dim_size(0);
    const int64_t out_rows = out_backprop.dim_size(1);
    const int64_t out_cols = out_backprop.dim_size(2);
    const int64_t out_depth = out_backprop.dim_size(3);

    OP_REQUIRES(context, row_seq_tensor.NumElements() > out_rows,
                errors::InvalidArgument("Given out_backprop shape ",
                                        out_backprop.shape().DebugString(),
                                        ", row_seq_tensor must have at least ",
                                        out_rows + 1, " elements, but got ",
                                        row_seq_tensor.NumElements()));
    OP_REQUIRES(context, col_seq_tensor.NumElements() > out_cols,
                errors::InvalidArgument("Given out_backprop shape ",
                                        out_backprop.shape().DebugString(),
                                        ", col_seq_tensor must have at least ",
                                        out_cols + 1, " elements, but got ",
                                        col_seq_tensor.NumElements()));

    auto row_seq_tensor_flat = row_seq_tensor.flat<int64>();
    auto col_seq_tensor_flat = col_seq_tensor.flat<int64>();
    auto orig_input_tensor_shape_flat = orig_input_tensor_shape.flat<int64>();

    const int64_t in_batch = orig_input_tensor_shape_flat(0);
    const int64_t in_rows = orig_input_tensor_shape_flat(1);
    const int64_t in_cols = orig_input_tensor_shape_flat(2);
    const int64_t in_depth = orig_input_tensor_shape_flat(3);

    constexpr int tensor_in_and_out_dims = 4;
    // Transform orig_input_tensor_shape into TensorShape
    TensorShape in_shape;
    for (auto i = 0; i < tensor_in_and_out_dims; ++i) {
      in_shape.AddDim(orig_input_tensor_shape_flat(i));
    }

    // Create intermediate in_backprop.
    Tensor in_backprop_tensor_temp;
    OP_REQUIRES_OK(context, context->forward_input_or_allocate_temp(
                                {0}, DataTypeToEnum<double>::v(), in_shape,
                                &in_backprop_tensor_temp));
    in_backprop_tensor_temp.flat<double>().setZero();
    // Transform 4D tensor to 2D matrix.
    EigenDoubleMatrixMap in_backprop_tensor_temp_mat(
        in_backprop_tensor_temp.flat<double>().data(), in_depth,
        in_cols * in_rows * in_batch);
    ConstEigenMatrixMap out_backprop_mat(out_backprop.flat<T>().data(),
                                         out_depth,
                                         out_cols * out_rows * out_batch);
    // Loop through each element of out_backprop and evenly distribute the
    // element to the corresponding pooling cell.
    const int64_t in_max_row_index = in_rows - 1;
    const int64_t in_max_col_index = in_cols - 1;
    for (int64_t b = 0; b < out_batch; ++b) {
      for (int64_t r = 0; r < out_rows; ++r) {
        const int64_t in_row_start = row_seq_tensor_flat(r);
        int64_t in_row_end = overlapping_ ? row_seq_tensor_flat(r + 1)
                                          : row_seq_tensor_flat(r + 1) - 1;
        in_row_end = std::min(in_row_end, in_max_row_index);
        for (int64_t c = 0; c < out_cols; ++c) {
          const int64_t in_col_start = col_seq_tensor_flat(c);
          int64_t in_col_end = overlapping_ ? col_seq_tensor_flat(c + 1)
                                            : col_seq_tensor_flat(c + 1) - 1;
          in_col_end = std::min(in_col_end, in_max_col_index);

          const int64_t num_elements_in_pooling_cell =
              (in_row_end - in_row_start + 1) * (in_col_end - in_col_start + 1);
          const int64_t out_index = (b * out_rows + r) * out_cols + c;
          // Now we can evenly distribute out_backprop(b, h, w, *) to
          // in_backprop(b, hs:he, ws:we, *).
          for (int64_t in_r = in_row_start; in_r <= in_row_end; ++in_r) {
            for (int64_t in_c = in_col_start; in_c <= in_col_end; ++in_c) {
              const int64_t in_index = (b * in_rows + in_r) * in_cols + in_c;
              // Walk through each channel (depth).
              for (int64_t d = 0; d < out_depth; ++d) {
                const double out_backprop_element = static_cast<double>(
                    out_backprop_mat.coeffRef(d, out_index));
                double& in_backprop_ref =
                    in_backprop_tensor_temp_mat.coeffRef(d, in_index);
                in_backprop_ref +=
                    out_backprop_element / num_elements_in_pooling_cell;
              }
            }
          }
        }
      }
    }

    // Depending on the type, cast double to type T.
    Tensor* in_backprop_tensor = nullptr;
    OP_REQUIRES_OK(context, context->forward_input_or_allocate_output(
                                {0}, 0, in_shape, &in_backprop_tensor));
    auto in_backprop_tensor_flat = in_backprop_tensor->flat<T>();
    auto in_backprop_tensor_temp_flat = in_backprop_tensor_temp.flat<double>();
    for (int64_t i = 0; i < in_backprop_tensor_flat.size(); ++i) {
      in_backprop_tensor_flat(i) =
          static_cast<T>(in_backprop_tensor_temp_flat(i));
    }
  }