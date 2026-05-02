Status ConcatShapeHelper(InferenceContext* c, int start_value_index,
                         int end_value_index, int dim_index) {
  ShapeHandle unused;
  TF_RETURN_IF_ERROR(c->WithRank(c->input(dim_index), 0, &unused));
  const Tensor* concat_dim_t = c->input_tensor(dim_index);
  if (concat_dim_t == nullptr) {
    // Return an unknown shape with same rank as inputs, or an unknown rank
    // if no input's rank is known.

    // Find rank.
    int32_t rank = InferenceContext::kUnknownRank;
    for (int i = start_value_index; i < end_value_index; ++i) {
      if (rank == InferenceContext::kUnknownRank) rank = c->Rank(c->input(i));
      if (rank != InferenceContext::kUnknownRank) {
        break;
      }
    }
    if (rank == InferenceContext::kUnknownRank) {
      c->set_output(0, c->UnknownShape());
      return Status::OK();
    } else if (rank == 0) {
      return errors::InvalidArgument(
          "Can't concatenate scalars (use tf.stack instead)");
    } else {
      for (int i = start_value_index; i < end_value_index; ++i) {
        // Check that all the inputs are of the correct rank.
        TF_RETURN_IF_ERROR(c->WithRank(c->input(i), rank, &unused));
      }
    }
    // Build result of <rank> different unknown dims.
    std::vector<DimensionHandle> dims;
    dims.reserve(rank);
    for (int i = 0; i < rank; ++i) dims.push_back(c->UnknownDim());
    c->set_output(0, c->MakeShape(dims));
    return Status::OK();
  }

  // Merge all the non-concat dims, and sum the concat dim to make an output
  // shape.
  int64_t concat_dim;
  if (concat_dim_t->dtype() == DT_INT32) {
    concat_dim = static_cast<int64_t>(concat_dim_t->flat<int32>()(0));
  } else {
    concat_dim = concat_dim_t->flat<int64_t>()(0);
  }

  // Minimum required number of dimensions.
  const int min_rank = concat_dim < 0 ? -concat_dim : concat_dim + 1;

  ShapeHandle output_before;
  ShapeHandle output_after;

  ShapeHandle input = c->input(end_value_index - 1);
  TF_RETURN_IF_ERROR(c->WithRankAtLeast(input, min_rank, &input));
  TF_RETURN_IF_ERROR(c->Subshape(input, 0, concat_dim, &output_before));
  DimensionHandle output_middle = c->Dim(input, concat_dim);
  if (concat_dim == -1) {
    output_after = c->Scalar();  // no dimensions.
  } else {
    TF_RETURN_IF_ERROR(c->Subshape(input, concat_dim + 1, &output_after));
  }

  for (int i = end_value_index - 2; i >= start_value_index; --i) {
    ShapeHandle before;
    ShapeHandle after;
    input = c->input(i);
    TF_RETURN_IF_ERROR(c->WithRankAtLeast(input, min_rank, &input));
    TF_RETURN_IF_ERROR(c->Subshape(input, 0, concat_dim, &before));
    DimensionHandle middle = c->Dim(input, concat_dim);
    if (concat_dim == -1) {
      after = c->Scalar();
    } else {
      TF_RETURN_IF_ERROR(c->Subshape(input, concat_dim + 1, &after));
    }

    TF_RETURN_IF_ERROR(c->Merge(before, output_before, &output_before));
    TF_RETURN_IF_ERROR(c->Add(output_middle, middle, &output_middle));
    TF_RETURN_IF_ERROR(c->Merge(after, output_after, &output_after));
  }

  ShapeHandle s;
  TF_RETURN_IF_ERROR(
      c->Concatenate(output_before, c->Vector(output_middle), &s));
  TF_RETURN_IF_ERROR(c->Concatenate(s, output_after, &s));
  c->set_output(0, s);
  return Status::OK();
}