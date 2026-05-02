  void Compute(OpKernelContext* context) override {
    const Tensor& input = context->input(0);
    const TensorShape& input_shape = input.shape();
    const int32 input_dims = input_shape.dims();

    const Tensor& segment_id = context->input(1);
    const TensorShape& segment_id_shape = segment_id.shape();
    const int32 segment_dims = segment_id_shape.dims();

    const Tensor& num_segments_tensor = context->input(2);
    auto num_segments = num_segments_tensor.scalar<NUM_SEGMENTS_TYPE>()();

    OP_REQUIRES(context, segment_dims != 0,
                errors::InvalidArgument("Segment_id cannot have rank 0"));

    OP_REQUIRES(
        context, segment_dims <= input_dims,
        errors::OutOfRange("Invalid segment_id rank ", segment_dims,
                           " for input with ", input_dims, " dimension(s)"));
    for (auto i = 0; i < segment_dims; i++) {
      OP_REQUIRES(
          context, segment_id_shape.dim_size(i) == input_shape.dim_size(i),
          errors::InvalidArgument(
              "Segment dimension is ", segment_id_shape.dim_size(i),
              " while input dimension is ", input_dims, " in rank ", i));
    }

    // Making output tensor.
    Tensor* output_tensor = nullptr;
    TensorShape output_shape =
        GetOutputShape(input_shape, segment_id_shape, num_segments);
    OP_REQUIRES_OK(context, context->allocate_output("output", output_shape,
                                                     &output_tensor));

    // Preparating flat tensors.
    auto output_flat = output_tensor->flat<tstring>();
    auto flat_segment_id = segment_id.flat<INDICES_TYPE>();
    auto flat_input = input.flat<tstring>();

    for (int i = 0; i < flat_segment_id.size(); i++) {
      OP_REQUIRES(
          context,
          ((flat_segment_id(i) < num_segments) && (flat_segment_id(i) >= 0)),
          errors::InvalidArgument(
              "segment_ids are not allowed to exceed num_segments or"
              " to have negative values."));
    }

    int64 big_stride;
    int64 small_stride;
    std::tie(big_stride, small_stride) =
        GetStrides<INDICES_TYPE>(input_shape, segment_id_shape);
    auto relative_offset_set =
        GetFlattenedRelativeOffsets<INDICES_TYPE>(small_stride, big_stride);
    for (auto start_offset = 0; start_offset < big_stride; start_offset++) {
      for (auto i = 0; i < relative_offset_set.size(); i++) {
        auto output_index = start_offset + flat_segment_id(i) * big_stride;
        auto offset = start_offset + relative_offset_set[i];
        if (output_flat(output_index).length() != 0)
          output_flat(output_index).append(separator_.c_str());
        output_flat(output_index).append(flat_input(offset));
      }
    }
  }