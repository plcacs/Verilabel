  void Compute(OpKernelContext* c) override {
    PartialTensorShape element_shape;
    OP_REQUIRES_OK(c, TensorShapeFromTensor(c->input(0), &element_shape));
    int32 num_elements = c->input(1).scalar<int32>()();
    OP_REQUIRES(c, num_elements >= 0,
                errors::InvalidArgument("The num_elements to reserve must be a "
                                        "non negative number, but got ",
                                        num_elements));
    TensorList output;
    output.element_shape = element_shape;
    output.element_dtype = element_dtype_;
    output.tensors().resize(num_elements, Tensor(DT_INVALID));
    Tensor* result;
    AllocatorAttributes attr;
    attr.set_on_host(true);
    OP_REQUIRES_OK(c, c->allocate_output(0, TensorShape{}, &result, attr));
    result->scalar<Variant>()() = std::move(output);
  }