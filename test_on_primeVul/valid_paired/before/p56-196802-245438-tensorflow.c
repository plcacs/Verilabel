  static void launch(OpKernelContext* context, const PoolParameters& params,
                     const Tensor& grad_in, const Tensor& argmax,
                     Tensor* grad_out, const bool include_batch_in_index) {
    const DeviceBase::CpuWorkerThreads& worker_threads =
        *(context->device()->tensorflow_cpu_worker_threads());

    auto shard = [&grad_in, &argmax, &grad_out, include_batch_in_index](
                     int64 start, int64 limit) {
      const int64 batch_size =
          GetTensorDim(grad_out->shape(), FORMAT_NHWC, 'N');
      const int64 output_size_per_batch = grad_out->NumElements() / batch_size;
      const int64 input_size_per_batch = grad_in.NumElements() / batch_size;

      {
        auto grad_out_flat = grad_out->flat<T>();
        auto argmax_flat = argmax.flat<int64>();
        auto grad_in_flat = grad_in.flat<T>();

        const int64 output_start = start * output_size_per_batch;
        const int64 output_end = limit * output_size_per_batch;
        EigenMatrixMap inputShard(grad_out_flat.data() + output_start, 1,
                                  output_end - output_start);
        inputShard.setConstant(T(0));

        const int input_start = start * input_size_per_batch;
        const int input_end = limit * input_size_per_batch;
        for (int64 index = input_start; index < input_end; index++) {
          int64 grad_out_index = argmax_flat(index);
          if (!include_batch_in_index) {
            const int64 cur_batch = index / input_size_per_batch;
            grad_out_index += cur_batch * output_size_per_batch;
          }
          CHECK(grad_out_index >= output_start && grad_out_index < output_end)
              << "Invalid output gradient index: " << grad_out_index << ", "
              << output_start << ", " << output_end;
          grad_out_flat(grad_out_index) += grad_in_flat(index);
        }
      }
    };

    const int64 batch_size = GetTensorDim(grad_out->shape(), FORMAT_NHWC, 'N');
    const int64 shard_cost = grad_out->NumElements() / batch_size;
    Shard(worker_threads.num_threads, worker_threads.workers, batch_size,
          shard_cost, shard);
  }