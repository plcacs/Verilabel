Status ConstantFolding::IsSimplifiableReshape(
    const NodeDef& node, const GraphProperties& properties) const {
  if (!IsReshape(node)) {
    return errors::Internal("Node ", node.name(), " is not a Reshape node");
  }
  if (2 > node.input_size()) {
    return errors::Internal("Node ", node.name(),
                            " must have at most 2 inputs but has ",
                            node.input_size());
  }
  const NodeDef* new_shape = node_map_->GetNode(node.input(1));
  if (!IsReallyConstant(*new_shape)) {
    return errors::Internal("Node ", node.name(), " has shape ",
                            new_shape->DebugString(),
                            " which is not a constant");
  }
  TensorVector outputs;
  auto outputs_cleanup = gtl::MakeCleanup([&outputs] {
    for (const auto& output : outputs) {
      delete output.tensor;
    }
  });

  Status s = EvaluateNode(*new_shape, TensorVector(), &outputs);
  if (!s.ok()) {
    return errors::Internal("Could not evaluate node ", node.name());
  }
  if (outputs.size() != 1) {
    return errors::Internal("Node ", node.name(),
                            " must have exactly 1 output but has ",
                            outputs.size());
  }

  const std::vector<OpInfo::TensorProperties>& props =
      properties.GetInputProperties(node.name());
  if (props.empty()) {
    return errors::Internal("Node ", node.name(), " has no properties");
  }
  const OpInfo::TensorProperties& prop = props[0];
  if (prop.dtype() == DT_INVALID) {
    return errors::Internal("Node ", node.name(), " has property ",
                            prop.DebugString(), " with invalid dtype");
  }
  const PartialTensorShape shape(prop.shape());
  if (!shape.IsFullyDefined()) {
    return errors::Internal("Node ", node.name(), " has property ",
                            prop.DebugString(), " with shape ",
                            shape.DebugString(), " which is not fully defined");
  }

  PartialTensorShape new_dims;
  if (outputs[0]->dtype() == DT_INT32) {
    std::vector<int32> shp;
    for (int i = 0; i < outputs[0]->NumElements(); ++i) {
      int32_t dim = outputs[0]->flat<int32>()(i);
      shp.push_back(dim);
    }
    s = TensorShapeUtils::MakeShape(shp, &new_dims);
    if (!s.ok()) return s;
  } else {
    std::vector<int64_t> shp;
    for (int i = 0; i < outputs[0]->NumElements(); ++i) {
      int64_t dim = outputs[0]->flat<int64_t>()(i);
      shp.push_back(dim);
    }
    s = TensorShapeUtils::MakeShape(shp, &new_dims);
    if (!s.ok()) return s;
  }

  if (!shape.IsCompatibleWith(new_dims)) {
    return errors::Internal("Expected shape ", shape.DebugString(),
                            "to be compatible with ", new_dims.DebugString());
  }

  return Status::OK();
}