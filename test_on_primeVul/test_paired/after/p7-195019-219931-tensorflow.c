Status ConstantFolding::EvaluateOneFoldable(const NodeDef& node,
                                            std::vector<NodeDef>* outputs,
                                            bool* result_too_large) {
  TensorVector inputs;
  TensorVector output_tensors;
  auto inputs_cleanup = gtl::MakeCleanup([&inputs, &output_tensors] {
    for (const auto& input : inputs) {
      delete input.tensor;
    }
    for (const auto& output : output_tensors) {
      if (output.tensor) {
        delete output.tensor;
      }
    }
  });

  size_t total_inputs_size = 0;
  for (const auto& input : node.input()) {
    const TensorId input_tensor = ParseTensorName(input);
    if (input_tensor.index() < 0) {
      // Control dependency
      break;
    }
    const NodeDef* input_node = node_map_->GetNode(input);
    if (!IsReallyConstant(*input_node)) {
      return Status(error::INVALID_ARGUMENT,
                    strings::StrCat("Can't fold ", node.name(), ", its ", input,
                                    " isn't constant"));
    }
    TF_RETURN_IF_ERROR(CheckAttrExists(*input_node, "value"));
    const TensorProto& raw_val = input_node->attr().at("value").tensor();
    if (raw_val.dtype() == DT_INVALID) {
      return Status(
          error::INVALID_ARGUMENT,
          strings::StrCat("A tensor in the input node, with TensorId of ",
                          input_tensor.ToString(),
                          " has a dtype of DT_INVALID."));
    }
    if (IsRefType(raw_val.dtype())) {
      return errors::InvalidArgument(
          "Not allowed to construct a tensor with reference dtype, got ",
          DataTypeString(raw_val.dtype()));
    }
    Tensor* value = new Tensor(raw_val.dtype(), raw_val.tensor_shape());
    if (!value->FromProto(raw_val)) {
      delete (value);
      return errors::InvalidArgument("Unable to make Tensor from proto for ",
                                     node.name(), " with shape ",
                                     raw_val.tensor_shape().DebugString());
    }
    inputs.emplace_back(value);
    total_inputs_size += value->TotalBytes();
  }

  TF_RETURN_IF_ERROR(EvaluateNode(node, inputs, &output_tensors));
  if (output_tensors.empty()) {
    return Status(error::INVALID_ARGUMENT, "Expected at least one output.");
  }

  outputs->resize(output_tensors.size());
  for (size_t i = 0; i < output_tensors.size(); i++) {
    string node_name = OptimizedNodeName(node, "-folded");
    if (output_tensors.size() > 1) {
      node_name = strings::StrCat(node_name, "-", i);
    }
    if (output_tensors[i].tensor) {
      Status s = CreateNodeDef(node_name, output_tensors[i], &outputs->at(i),
                               total_inputs_size);
      if (!s.ok()) {
        *result_too_large = true;
        return s;
      }
    } else {
      // Create an empty NodeDef to identify dead outputs (e.g. the output of a
      // switch that's not selected by the switch predicate).
      outputs->at(i) = NodeDef();
    }
  }
  return Status::OK();
}