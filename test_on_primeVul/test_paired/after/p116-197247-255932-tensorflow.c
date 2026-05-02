Status ShapeRefiner::InferShapesForFunctionSubNode(
    const Node* node, InferenceContext* outer_context) {
  TF_RETURN_IF_ERROR(AddNodeInternal(node, outer_context));
  InferenceContext* node_context = CHECK_NOTNULL(GetContext(node));

  if (StringPiece(node->type_string()) == kArgOp) {
    // Handle special node: function input.
    // Shapes for these nodes are provided in the outer inference
    // context.

    int index;
    TF_RETURN_IF_ERROR(GetNodeAttr(AttrSlice(node->def()), "index", &index));

    if (index < 0 || outer_context->num_inputs() <= index) {
      return errors::Internal(
          "Function instantiation included invalid input index: ", index,
          " not in [0, ", outer_context->num_inputs(), ").");
    }

    // TODO(b/134547156): TEMPORARY WORKAROUND. If input shape handle is not set
    // in outer context, set _Arg node output shape to unknown.
    if (outer_context->input(index).SameHandle(ShapeHandle())) {
      VLOG(1) << "Function instantiation has undefined input shape at "
              << "index: " << index << " in the outer inference context.";
      node_context->set_output(0, node_context->UnknownShape());
    } else {
      node_context->set_output(0, outer_context->input(index));
    }

    auto* resource = outer_context->input_handle_shapes_and_types(index);
    if (resource) {
      node_context->set_output_handle_shapes_and_types(0, *resource);
    }
  } else if (StringPiece(node->type_string()) == kRetvalOp) {
    // Handle special node: function output.
    // Shapes inferred for these nodes go into the outer inference
    // context.

    int index;
    TF_RETURN_IF_ERROR(GetNodeAttr(AttrSlice(node->def()), "index", &index));

    if (index < 0 || outer_context->num_outputs() <= index) {
      return errors::Internal(
          "Function instantiation included invalid output index: ", index,
          " not in [0, ", outer_context->num_outputs(), ").");
    }

    // outer_context outlives node_context, therefore we need to create
    // a new shape handle owned by outer_context instead.
    ShapeHandle handle;
    TensorShapeProto proto;
    node_context->ShapeHandleToProto(node_context->input(0), &proto);
    TF_RETURN_IF_ERROR(outer_context->MakeShapeFromShapeProto(proto, &handle));
    outer_context->set_output(index, handle);

    const std::vector<ShapeAndType>* resource =
        node_context->input_handle_shapes_and_types(0);
    if (resource) {
      // `ShapesAndType`s contain `ShapeHandle`s.  These `ShapeHandle`s point
      // to `Shape`s that are owned by a different inference context too.  We
      // need to copy them to the outer context to prevent them from being
      // destroyed before they are used.
      std::vector<ShapeAndType> copied_shapes_and_types;
      for (auto& shape_and_type : *resource) {
        ShapeHandle handle;
        TensorShapeProto proto;
        node_context->ShapeHandleToProto(shape_and_type.shape, &proto);
        TF_RETURN_IF_ERROR(
            outer_context->MakeShapeFromShapeProto(proto, &handle));
        copied_shapes_and_types.push_back(
            ShapeAndType(handle, shape_and_type.dtype, shape_and_type.type));
      }

      outer_context->set_output_handle_shapes_and_types(
          index, copied_shapes_and_types);
    }
  }

  return Status::OK();
}