  void Compute(OpKernelContext* ctx) override {
    try {
      const Tensor& input = ctx->input(kInputTensorIndex);
      const Tensor& input_min_vec = ctx->input(kInputMinVecIndex);
      float* input_min_vec_data = (float*)const_cast<void*>(
          static_cast<const void*>(input_min_vec.flat<float>().data()));
      const Tensor& input_max_vec = ctx->input(kInputMaxVecIndex);
      float* input_max_vec_data = (float*)const_cast<void*>(
          static_cast<const void*>(input_max_vec.flat<float>().data()));

      const Tensor& input_requested_min = ctx->input(this->kRequestMinIndex);
      const float input_requested_min_float =
          input_requested_min.flat<float>()(0);
      const Tensor& input_requested_max = ctx->input(this->kRequestMaxIndex);
      const float input_requested_max_float =
          input_requested_max.flat<float>()(0);

      size_t depth = input_min_vec.NumElements();
      OP_REQUIRES(
          ctx, input.dims() == 4,
          errors::InvalidArgument("Current RequantizePerChannel operator"
                                  "supports 4D tensors only."));
      OP_REQUIRES(
          ctx, input_min_vec.dim_size(0) == depth,
          errors::InvalidArgument("input_min has incorrect size, expected ",
                                  depth, " was ", input_min_vec.dim_size(0)));
      OP_REQUIRES(
          ctx, input_max_vec.dim_size(0) == depth,
          errors::InvalidArgument("input_max has incorrect size, expected ",
                                  depth, " was ", input_max_vec.dim_size(0)));

      if (out_type_ == DT_QINT8) DCHECK(input_requested_min_float < 0.0f);

      const float factor = (out_type_ == DT_QINT8) ? 127.0f : 255.0f;
      const float requested_min_max =
          std::max(std::abs(input_requested_min_float),
                   std::abs(input_requested_max_float));
      Tensor* output = nullptr;
      OP_REQUIRES_OK(ctx, ctx->allocate_output(kOutputTensorIndex,
                                               input.shape(), &output));

      std::vector<float> scales(depth);
      for (int i = 0; i < depth; ++i) {
        float min_max_from_vec = std::max(std::abs(input_min_vec_data[i]),
                                          std::abs(input_max_vec_data[i]));
        scales[i] = factor * (min_max_from_vec / requested_min_max /
                              static_cast<float>(1L << 31));
      }

      mkldnn::primitive_attr reorder_attr;
      reorder_attr.set_output_scales(2, scales);

      memory::dims dims_mkl_order =
          TFShapeToMklDnnDimsInNCHW(input.shape(), FORMAT_NHWC);
      memory::desc input_md = memory::desc(dims_mkl_order, MklDnnType<qint32>(),
                                           memory::format_tag::nhwc);
      memory::desc output_md =
          (out_type_ == DT_QINT8)
              ? memory::desc(dims_mkl_order, MklDnnType<qint8>(),
                             memory::format_tag::nhwc)
              : memory::desc(dims_mkl_order, MklDnnType<quint8>(),
                             memory::format_tag::nhwc);

      void* input_buf =
          static_cast<void*>(const_cast<qint32*>(input.flat<qint32>().data()));
      void* output_buf;
      if (out_type_ == DT_QINT8) {
        output_buf = static_cast<void*>(
            const_cast<qint8*>(output->flat<qint8>().data()));
      } else {
        output_buf = static_cast<void*>(
            const_cast<quint8*>(output->flat<quint8>().data()));
      }

      std::unique_ptr<memory> input_mem_prim(
          new memory(input_md, cpu_engine_, input_buf));
      std::unique_ptr<memory> output_mem_prim(
          new memory(output_md, cpu_engine_, output_buf));

      mkldnn::reorder::primitive_desc reorder_pd =
          ReorderPd(cpu_engine_, input_mem_prim->get_desc(), cpu_engine_,
                    output_mem_prim->get_desc(), reorder_attr);
      std::shared_ptr<stream> reorder_stream;
      MklDnnThreadPool eigen_tp(ctx);
      reorder_stream.reset(CreateStream(&eigen_tp, cpu_engine_));
      std::unordered_map<int, mkldnn::memory> reorder_args = {
          {MKLDNN_ARG_FROM, *input_mem_prim},
          {MKLDNN_ARG_TO, *output_mem_prim}};
      std::unique_ptr<mkldnn::primitive> reorder_prim(
          new mkldnn::reorder(reorder_pd));
      reorder_prim->execute(*reorder_stream, reorder_args);

      Tensor* output_min = nullptr;
      Tensor* output_max = nullptr;
      OP_REQUIRES_OK(ctx,
                     ctx->allocate_output(kOutputMinIndex, {}, &output_min));
      OP_REQUIRES_OK(ctx,
                     ctx->allocate_output(kOutputMaxIndex, {}, &output_max));

      output_min->flat<float>()(0) = input_requested_min_float;
      output_max->flat<float>()(0) = input_requested_max_float;
    } catch (mkldnn::error& e) {
      string error_msg = "Status: " + std::to_string(e.status) +
                         ", message: " + std::string(e.message) + ", in file " +
                         std::string(__FILE__) + ":" + std::to_string(__LINE__);
      OP_REQUIRES_OK(
          ctx, errors::Aborted("Operation received an exception:", error_msg));
    }
  }