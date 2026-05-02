  void Compute(OpKernelContext* context) override {
    // Get inputs
    const Tensor& input_tensor = context->input(0);
    const auto input_tensor_flat = input_tensor.flat<int32>();
    const Tensor& input_splits = context->input(1);
    const auto input_splits_flat = input_splits.flat<SPLITS_TYPE>();

    // Operation will treat first argument in input_splits as if it were zero
    // regardless of its actual value since splits should begin with zero and
    // end with the length of the input values vector.
    OP_REQUIRES(
        context, input_splits_flat(0) == 0,
        errors::InvalidArgument("First value in input_splits must be zero."));
    OP_REQUIRES(context,
                input_splits_flat(input_splits_flat.size() - 1) ==
                    input_tensor_flat.size(),
                errors::InvalidArgument("Last value in input_splits must be "
                                        "equal to length of input_tensor."));
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
      OP_REQUIRES(
          context, input_splits_flat(i - 1) <= input_splits_flat(i),
          errors::InvalidArgument(
              "Values in input_splits must be equal or in ascending order."));
      OP_REQUIRES(
          context, input_splits_flat(i) <= input_tensor_flat.size(),
          errors::InvalidArgument("Values in input_splits must be less than or "
                                  "equal to input_tensor length."));
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