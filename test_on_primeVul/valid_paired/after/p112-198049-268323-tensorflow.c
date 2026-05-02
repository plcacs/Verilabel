Status GraphConstructor::MakeEdge(Node* src, int output_index, Node* dst,
                                  int input_index) {
  if (output_index >= src->num_outputs()) {
    return errors::InvalidArgument(
        "Output ", output_index, " of node ", src->name(),
        " does not exist. Node only has ", src->num_outputs(), " outputs.");
  }
  if (input_index >= dst->num_inputs()) {
    return errors::InvalidArgument(
        "Input ", input_index, " of node ", dst->name(),
        " does not exist. Node only has ", dst->num_inputs(), " inputs.");
  }

  DataType src_out = src->output_type(output_index);
  DataType dst_in = dst->input_type(input_index);
  if (!TypesCompatible(dst_in, src_out)) {
    return errors::InvalidArgument(
        "Input ", input_index, " of node ", dst->name(), " was passed ",
        DataTypeString(src_out), " from ", src->name(), ":", output_index,
        " incompatible with expected ", DataTypeString(dst_in), ".");
  }
  g_->AddEdge(src, output_index, dst, input_index);
  return Status::OK();
}