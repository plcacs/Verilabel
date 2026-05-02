  void Compute(OpKernelContext* context) override {
    // Get inputs
    const Tensor& input_tensor = context->input(0);
    const auto input_tensor_flat = input_tensor.flat<int32>();
    const Tensor& input_splits = context->input(1);
    const auto input_splits_flat = input_splits.flat<SPLITS_TYPE>();

    // Since we limit to a 2-D input (flat_values of rank 1 and a single splits
    // tensor), our output dimension will be 1 with it's size equal to the
    // number of splits (outer dimension or ragged tensor).
    TensorShape output_shape({input_splits.dim_size(0) - 1});
    Tensor* output_tensor;
    OP_REQUIRES_OK(context, context->allocate_output("output", output_shape,
                                                     &output_tensor));
    auto output_tensor_flat = output_tensor->flat<tstring>();

    // Use a single index over the flattened input values tensor.
    int idx = 0;
    // Loop through our split dimension to create a new string at each split.
    for (int i = 1; i < input_splits_flat.size(); ++i) {
      icu::UnicodeString unicode_string;
      icu::UnicodeStringAppendable appendable_unicode_string(unicode_string);
      for (; idx < input_splits_flat(i); ++idx) {
        int32 code_point = input_tensor_flat(idx);
        // Check for invalid code point
        if (!U_IS_UNICODE_CHAR(code_point)) {
          if (error_options_.error_on_malformatting) {
            context->CtxFailure(errors::InvalidArgument(
                "Code point is out of range for Unicode, or a noncharacter."));
            return;
          } else if (!error_options_.elide_replacement) {
            code_point = error_options_.subst;
          }
        }
        appendable_unicode_string.appendCodePoint(code_point);
      }
      // Encode our string and save in the output.
      tstring result;
      Encode(encoding_, unicode_string, &result);
      output_tensor_flat(i - 1) = std::move(result);
    }
  }