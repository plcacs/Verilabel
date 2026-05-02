  Status SetUnknownShape(const NodeDef* node, int output_port) {
    shape_inference::ShapeHandle shape =
        GetUnknownOutputShape(node, output_port);
    InferenceContext* ctx = GetContext(node);
    if (ctx == nullptr) {
      return errors::InvalidArgument("SetUnknownShape: Missing context");
    }
    if (output_port < 0 || output_port >= ctx->num_outputs()) {
      return errors::InvalidArgument(
          "SetUnknownShape: output_port must be in [0, ", ctx->num_outputs(),
          ") but was ", output_port);
    }
    ctx->set_output(output_port, shape);
    return Status::OK();
  }