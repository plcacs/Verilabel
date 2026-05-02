  void Compute(OpKernelContext* c) override {
    const Tensor& tag = c->input(0);
    OP_REQUIRES(c, TensorShapeUtils::IsScalar(tag.shape()),
                errors::InvalidArgument("tag must be scalar"));
    const Tensor& tensor = c->input(1);
    const Tensor& serialized_summary_metadata_tensor = c->input(2);

    Summary s;
    Summary::Value* v = s.add_value();
    v->set_tag(string(tag.scalar<tstring>()()));  // NOLINT

    if (tensor.dtype() == DT_STRING) {
      // tensor_util.makeNdarray doesn't work for strings in tensor_content
      tensor.AsProtoField(v->mutable_tensor());
    } else {
      tensor.AsProtoTensorContent(v->mutable_tensor());
    }

    ParseFromTString(serialized_summary_metadata_tensor.scalar<tstring>()(),
                     v->mutable_metadata());

    Tensor* summary_tensor = nullptr;
    OP_REQUIRES_OK(c, c->allocate_output(0, TensorShape({}), &summary_tensor));
    CHECK(SerializeToTString(s, &summary_tensor->scalar<tstring>()()));
  }