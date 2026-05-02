  void Compute(OpKernelContext* context) override {
    // There are two steps when calculating gradient for FractionalMaxPool.
    // 1) Walk through the process of calculating fractional pooling given
    //    pooling region; however, in the process, keep track of where the max
    //    element comes from. (arg_max)
    // 2) Populate the value of out_backprop to where arg_max indicates. If
    //    we support overlapping, it is likely to have multiple out_backprop[i]
    //    propagates back to the same arg_max value.
    typedef Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>>
        ConstEigenMatrixMap;
    typedef Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>>
        EigenMatrixMap;
    typedef Eigen::Map<Eigen::Matrix<int64, Eigen::Dynamic, Eigen::Dynamic>>
        EigenIndexMatrixMap;

    const Tensor& tensor_in = context->input(0);
    const Tensor& tensor_out = context->input(1);
    const Tensor& out_backprop = context->input(2);
    const Tensor& height_seq_tensor = context->input(3);
    const Tensor& width_seq_tensor = context->input(4);

    // Just to make it similar to FractionalMaxPoolOp.
    constexpr int tensor_in_and_out_dims = 4;
    std::vector<int64> input_size(tensor_in_and_out_dims);
    std::vector<int64> output_size(tensor_in_and_out_dims);
    for (int i = 0; i < tensor_in_and_out_dims; ++i) {
      input_size[i] = tensor_in.dim_size(i);
    }
    for (int i = 0; i < tensor_in_and_out_dims; ++i) {
      output_size[i] = tensor_out.dim_size(i);
    }

    // ---------
    // Step 1
    // ---------
    Tensor tensor_out_dup;
    OP_REQUIRES_OK(context, context->forward_input_or_allocate_temp(
                                {1}, DataTypeToEnum<T>::v(), tensor_out.shape(),
                                &tensor_out_dup));
    Tensor tensor_out_arg_max;
    OP_REQUIRES_OK(context, context->allocate_temp(DataTypeToEnum<int64>::v(),
                                                   tensor_out.shape(),
                                                   &tensor_out_arg_max));
    // Find arg_max for each tensor_out
    ConstEigenMatrixMap tensor_in_mat(
        tensor_in.flat<T>().data(), input_size[3],
        input_size[2] * input_size[1] * input_size[0]);
    EigenMatrixMap tensor_out_dup_mat(
        tensor_out_dup.flat<T>().data(), output_size[3],
        output_size[2] * output_size[1] * output_size[0]);
    EigenIndexMatrixMap tensor_out_arg_max_mat(
        tensor_out_arg_max.flat<int64>().data(), output_size[3],
        output_size[2] * output_size[1] * output_size[0]);

    tensor_out_arg_max.flat<int64>().setConstant(kInvalidMaxPoolingIndex);
    // Initializes the duplicate output tensor with MIN<T>.
    tensor_out_dup.flat<T>().setConstant(Eigen::NumTraits<T>::lowest());

    auto height_seq_tensor_flat = height_seq_tensor.flat<int64>();
    auto width_seq_tensor_flat = width_seq_tensor.flat<int64>();

    // Now walk through the process of fractional max pooling again.
    // For both input and output,
    // 0: batch
    // 1: height / row
    // 2: width / col
    // 3: depth / channel
    const int64 height_max = input_size[1] - 1;
    const int64 width_max = input_size[2] - 1;
    for (int64 b = 0; b < input_size[0]; ++b) {
      // height sequence.
      for (int64 hs = 0; hs < height_seq_tensor.dim_size(0) - 1; ++hs) {
        // height start and end.
        const int64 height_start = height_seq_tensor_flat(hs);
        int64 height_end = overlapping_ ? height_seq_tensor_flat(hs + 1)
                                        : height_seq_tensor_flat(hs + 1) - 1;
        height_end = std::min(height_end, height_max);

        // width sequence.
        for (int64 ws = 0; ws < width_seq_tensor.dim_size(0) - 1; ++ws) {
          const int64 out_index =
              (b * output_size[1] + hs) * output_size[2] + ws;
          // width start and end.
          const int64 width_start = width_seq_tensor_flat(ws);
          int64 width_end = overlapping_ ? width_seq_tensor_flat(ws + 1)
                                         : width_seq_tensor_flat(ws + 1) - 1;
          width_end = std::min(width_end, width_max);
          for (int64 h = height_start; h <= height_end; ++h) {
            for (int64 w = width_start; w <= width_end; ++w) {
              const int64 in_index =
                  (b * input_size[1] + h) * input_size[2] + w;
              // Walk through each channel (depth).
              for (int64 d = 0; d < input_size[3]; ++d) {
                const T& input_ref = tensor_in_mat.coeffRef(d, in_index);
                T& output_ref = tensor_out_dup_mat.coeffRef(d, out_index);
                int64& out_arg_max_ref =
                    tensor_out_arg_max_mat.coeffRef(d, out_index);
                if (output_ref < input_ref ||
                    out_arg_max_ref == kInvalidMaxPoolingIndex) {
                  output_ref = input_ref;
                  int input_offset = in_index * input_size[3] + d;
                  out_arg_max_ref = input_offset;
                }
              }
            }
          }
        }
      }
    }

    // Check tensor_out_dup is the same as tensor_out.
    ConstEigenMatrixMap tensor_out_mat(
        tensor_out.flat<T>().data(), output_size[3],
        output_size[2] * output_size[1] * output_size[0]);
    const int64 num_reshaped_cols =
        output_size[2] * output_size[1] * output_size[0];
    for (int64 i = 0; i < num_reshaped_cols; ++i) {
      for (int64 j = 0; j < output_size[3]; ++j) {
        DCHECK_EQ(tensor_out_dup_mat(j, i), tensor_out_mat(j, i));
      }
    }

    Tensor* output = nullptr;
    OP_REQUIRES_OK(context, context->forward_input_or_allocate_output(
                                {0}, 0, tensor_in.shape(), &output));
    output->flat<T>().setZero();

    auto out_backprop_flat = out_backprop.flat<T>();
    auto input_backprop_flat = output->flat<T>();
    auto out_arg_max_flat = tensor_out_arg_max.flat<int64>();
    int num_total_outputs = out_backprop_flat.size();
    int num_total_inputs = input_backprop_flat.size();

    for (int index = 0; index < num_total_outputs; ++index) {
      int input_backprop_index = out_arg_max_flat(index);
      // According to maxpooling_op.cc, the performance impact below is small.
      CHECK(input_backprop_index >= 0 &&
            input_backprop_index < num_total_inputs)
          << "Invalid input backprop index: " << input_backprop_index << ", "
          << num_total_inputs;
      input_backprop_flat(input_backprop_index) += out_backprop_flat(index);
    }
  }