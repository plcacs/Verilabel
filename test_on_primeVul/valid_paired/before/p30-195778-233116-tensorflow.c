  void ComputeEasyCases(OpKernelContext* context, bool* done,
                        std::vector<Tlen>* split_sizes_vec) {
    const int32_t num_split = context->num_outputs();
    const Tensor& input = context->input(0);
    const TensorShape& input_shape = input.shape();
    const Tensor& split_tensor = context->input(1);
    const Tensor& split_dim_tensor = context->input(2);

    OP_REQUIRES(context, split_dim_tensor.NumElements() == 1,
                errors::InvalidArgument("split_dim_tensor must have "
                                        "exactly one element."));

    const int32_t split_dim_orig = split_dim_tensor.flat<int32>()(0);
    const int32_t split_dim =
        split_dim_orig < 0 ? split_dim_orig + input.dims() : split_dim_orig;

    OP_REQUIRES(
        context,
        split_tensor.dims() == 1 && split_tensor.NumElements() == num_split,
        errors::InvalidArgument("size of the split_tensor must be 1-D and have "
                                "the same elements as outputs got ",
                                split_tensor.dims(), " -D and ",
                                split_tensor.NumElements(), " elements"));

    auto split_sizes_d = split_tensor.vec<Tlen>();

    split_sizes_vec->resize(split_sizes_d.size());

    std::copy(split_sizes_d.data(), split_sizes_d.data() + split_sizes_d.size(),
              split_sizes_vec->begin());

    OP_REQUIRES(
        context, num_split > 0,
        errors::InvalidArgument(
            "Number of ways to split should be > 0, but got ", num_split));

    OP_REQUIRES(
        context, 0 <= split_dim && split_dim < input.dims(),
        errors::InvalidArgument("-input rank(-", input.dims(),
                                ") <= split_dim < input rank (", input.dims(),
                                "), but got ", split_dim_orig));

    Tlen input_size_split_dim = input_shape.dim_size(split_dim);

    // Special case 1: num_split == 1. Nothing to do.
    if (num_split == 1) {
      context->set_output(0, context->input(0));
      OP_REQUIRES(
          context, (*split_sizes_vec)[0] == input_size_split_dim,
          errors::InvalidArgument("If there is only one output, it must have "
                                  "the same size as the input. Input size: ",
                                  input_size_split_dim,
                                  " output size: ", (*split_sizes_vec)[0]));
      *done = true;
      return;
    }

    // Determine sizes of output, in case of a -1 input value
    int neg_one_dim = -1;
    Tlen determined_size = 0;
    for (int d = 0; d < split_sizes_vec->size(); ++d) {
      Tlen size = (*split_sizes_vec)[d];

      if (size == -1) {
        OP_REQUIRES(context, neg_one_dim == -1,
                    errors::InvalidArgument("There can only be one -1 in the "
                                            "input."));
        neg_one_dim = d;
      } else {
        determined_size += size;
      }
    }

    OP_REQUIRES(
        context,
        (neg_one_dim == -1 && determined_size == input_size_split_dim) ||
            (neg_one_dim >= 0 && determined_size <= input_size_split_dim),
        errors::InvalidArgument("Determined shape must either match "
                                "input shape along split_dim exactly if "
                                "fully specified, or be less than the size of "
                                "the input along split_dim if not fully "
                                "specified.  Got: ",
                                determined_size));

    if (neg_one_dim >= 0) {
      (*split_sizes_vec)[neg_one_dim] = input_size_split_dim - determined_size;
    }

    // Special case 2: split along the 1st dimension. The requirements are that
    // either we are splitting the outer dimension of two or more such that
    // every outer subpart is aligned or that the split sizes mean that they are
    // always aligned. In these cases, we can share the underlying buffer.
    //
    // Apply this optimization conservatively: if input is aligned,
    // the resulting tensors must be aligned. It's conservative
    // because if the immediate consumer of the resulting tensors are
    // not using eigen for computation, its perfectly fine to avoid
    // the copying.
    if (SplitHasAlignedOutputsInFirstDimension(
            input_shape, split_dim, absl::MakeConstSpan(*split_sizes_vec))) {
      Tlen start = 0;
      for (int i = 0; i < num_split; ++i) {
        context->set_output(i,
                            input.Slice(start, start + (*split_sizes_vec)[i]));
        start += (*split_sizes_vec)[i];
      }
      *done = true;
      return;
    }
  }