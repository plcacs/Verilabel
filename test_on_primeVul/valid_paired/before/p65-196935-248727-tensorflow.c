static void SpatialMaxPoolWithArgMaxHelper(
    OpKernelContext* context, Tensor* output, Tensor* output_arg_max,
    Tensor* input_backprop, const Tensor& tensor_in, const Tensor& out_backprop,
    const PoolParameters& params, const bool include_batch_in_index) {
  if (input_backprop != nullptr) {
    OP_REQUIRES(
        context, include_batch_in_index,
        errors::Internal(
            "SpatialMaxPoolWithArgMaxHelper requires include_batch_in_index "
            "to be True when input_backprop != nullptr"));
    OP_REQUIRES(
        context, (std::is_same<Targmax, int64>::value),
        errors::Internal("SpatialMaxPoolWithArgMaxHelper requires Targmax "
                         "to be int64 when input_backprop != nullptr"));
  }

  typedef Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>>
      ConstEigenMatrixMap;
  typedef Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>>
      EigenMatrixMap;
  typedef Eigen::Map<Eigen::Matrix<Targmax, Eigen::Dynamic, Eigen::Dynamic>>
      EigenIndexMatrixMap;

  ConstEigenMatrixMap in_mat(
      tensor_in.flat<T>().data(), params.depth,
      params.tensor_in_cols * params.tensor_in_rows * params.tensor_in_batch);
  EigenMatrixMap out_mat(
      output->flat<T>().data(), params.depth,
      params.out_width * params.out_height * params.tensor_in_batch);
  EigenIndexMatrixMap out_arg_max_mat(
      output_arg_max->flat<Targmax>().data(), params.depth,
      params.out_width * params.out_height * params.tensor_in_batch);

  const DeviceBase::CpuWorkerThreads& worker_threads =
      *(context->device()->tensorflow_cpu_worker_threads());

  // The following code basically does the following:
  // 1. Flattens the input and output tensors into two dimensional arrays.
  //    tensor_in_as_matrix:
  //      depth by (tensor_in_cols * tensor_in_rows * tensor_in_batch)
  //    output_as_matrix:
  //      depth by (out_width * out_height * tensor_in_batch)
  //
  // 2. Walks through the set of columns in the flattened tensor_in_as_matrix,
  //    and updates the corresponding column(s) in output_as_matrix with the
  //    max value.
  auto shard = [&params, &in_mat, &out_mat, &out_arg_max_mat, &input_backprop,
                &output_arg_max, &out_backprop,
                include_batch_in_index](int64 start, int64 limit) {
    const int32 depth = params.depth;
    const int32 in_rows = params.tensor_in_rows;
    const int32 in_cols = params.tensor_in_cols;
    const int32 pad_top = params.pad_top;
    const int32 pad_left = params.pad_left;
    const int32 window_rows = params.window_rows;
    const int32 window_cols = params.window_cols;
    const int32 row_stride = params.row_stride;
    const int32 col_stride = params.col_stride;
    const int32 out_height = params.out_height;
    const int32 out_width = params.out_width;

    {
      // Initializes the output tensor with MIN<T>.
      const int32 output_image_size = out_height * out_width * depth;
      EigenMatrixMap out_shard(out_mat.data() + start * output_image_size, 1,
                               (limit - start) * output_image_size);
      out_shard.setConstant(Eigen::NumTraits<T>::lowest());
      EigenIndexMatrixMap out_arg_max_shard(
          out_arg_max_mat.data() + start * output_image_size, 1,
          (limit - start) * output_image_size);
      out_arg_max_shard.setConstant(kInvalidMaxPoolingIndex);
    }

    for (int64 b = start; b < limit; ++b) {
      for (int h = 0; h < in_rows; ++h) {
        for (int w = 0; w < in_cols; ++w) {
          // (h_start, h_end) * (w_start, w_end) is the range that the input
          // vector projects to.
          const int hpad = h + pad_top;
          const int wpad = w + pad_left;
          const int h_start =
              (hpad < window_rows) ? 0 : (hpad - window_rows) / row_stride + 1;
          const int h_end = std::min(hpad / row_stride + 1, out_height);
          const int w_start =
              (wpad < window_cols) ? 0 : (wpad - window_cols) / col_stride + 1;
          const int w_end = std::min(wpad / col_stride + 1, out_width);
          // compute elementwise max
          const int64 in_index = (b * in_rows + h) * in_cols + w;
          for (int ph = h_start; ph < h_end; ++ph) {
            const int64 out_index_base = (b * out_height + ph) * out_width;
            for (int pw = w_start; pw < w_end; ++pw) {
              const int64 out_index = out_index_base + pw;
              /// NOTES(zhengxq): not using the eigen matrix operation for
              /// now.
              for (int d = 0; d < depth; ++d) {
                const T& input_ref = in_mat.coeffRef(d, in_index);
                T& output_ref = out_mat.coeffRef(d, out_index);
                Targmax& out_arg_max_ref =
                    out_arg_max_mat.coeffRef(d, out_index);
                if (output_ref < input_ref ||
                    out_arg_max_ref == kInvalidMaxPoolingIndex) {
                  output_ref = input_ref;
                  if (include_batch_in_index) {
                    out_arg_max_ref = in_index * depth + d;
                  } else {
                    out_arg_max_ref = (h * in_cols + w) * depth + d;
                  }
                }
              }
            }
          }
        }
      }
    }

    if (input_backprop != nullptr) {
      auto input_backprop_flat = input_backprop->flat<T>();
      auto out_arg_max_flat = output_arg_max->flat<int64>();
      auto out_backprop_flat = out_backprop.flat<T>();

      // Initialize output to 0.
      const int64 in_size = in_rows * in_cols * depth;
      const int64 in_start = start * in_size;
      const int64 in_end = limit * in_size;
      EigenMatrixMap in_shard(input_backprop_flat.data() + in_start, 1,
                              in_end - in_start);
      in_shard.setConstant(T(0));

      // Backpropagate.
      const int out_size = out_height * out_width * depth;
      const int out_start = start * out_size;
      const int out_end = limit * out_size;
      for (int index = out_start; index < out_end; ++index) {
        int input_backprop_index = out_arg_max_flat(index);
        // Although this check is in the inner loop, it is worth its value
        // so we don't end up with memory corruptions. Our benchmark shows that
        // the performance impact is quite small
        // CHECK(input_backprop_index >= in_start && input_backprop_index <
        // in_end)
        FastBoundsCheck(input_backprop_index - in_start, in_end - in_start);
        input_backprop_flat(input_backprop_index) += out_backprop_flat(index);
      }
    }
  };

  const int64 shard_cost = params.tensor_in_rows * params.tensor_in_cols *
                           params.depth * params.window_rows *
                           params.window_cols;
  Shard(worker_threads.num_threads, worker_threads.workers,
        params.tensor_in_batch, shard_cost, shard);
}