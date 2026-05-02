  void Compute(OpKernelContext* context) override {
    const float in_min = context->input(2).flat<float>()(0);
    const float in_max = context->input(3).flat<float>()(0);

    ImageResizerState st(align_corners_, false);
    st.ValidateAndCreateOutput(context);

    if (!context->status().ok()) return;

    // Return if the output is empty.
    if (st.output->NumElements() == 0) return;

    typename TTypes<T, 4>::ConstTensor image_data(
        context->input(0).tensor<T, 4>());
    typename TTypes<T, 4>::Tensor output_data(st.output->tensor<T, 4>());

    ResizeBilinear<T>(image_data, st.height_scale, st.width_scale, in_min,
                      in_max, half_pixel_centers_, &output_data);
    Tensor* out_min = nullptr;
    OP_REQUIRES_OK(context, context->allocate_output(1, {}, &out_min));
    out_min->flat<float>()(0) = in_min;

    Tensor* out_max = nullptr;
    OP_REQUIRES_OK(context, context->allocate_output(2, {}, &out_max));
    out_max->flat<float>()(0) = in_max;
  }