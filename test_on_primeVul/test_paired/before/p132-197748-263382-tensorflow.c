Status TransposeShapeFn(InferenceContext* c) {
  ShapeHandle input = c->input(0);
  ShapeHandle perm_shape = c->input(1);
  const Tensor* perm = c->input_tensor(1);
  DimensionHandle perm_elems = c->NumElements(perm_shape);
  // If we don't have rank information on the input or value information on
  // perm we can't return any shape information, otherwise we have enough
  // information to at least find the rank of the output.
  if (!c->RankKnown(input) && !c->ValueKnown(perm_elems) && perm == nullptr) {
    c->set_output(0, c->UnknownShape());
    return Status::OK();
  }

  // Find our value of the rank.
  int64_t rank;
  if (c->RankKnown(input)) {
    rank = c->Rank(input);
  } else if (c->ValueKnown(perm_elems)) {
    rank = c->Value(perm_elems);
  } else {
    rank = perm->NumElements();
  }
  if (!c->RankKnown(input) && rank < 2) {
    // A permutation array containing a single element is ambiguous. It could
    // indicate either a scalar or a 1-dimensional array, both of which the
    // transpose op returns unchanged.
    c->set_output(0, input);
    return Status::OK();
  }

  std::vector<DimensionHandle> dims;
  dims.resize(rank);
  TF_RETURN_IF_ERROR(c->WithRank(input, rank, &input));
  // Ensure that perm is a vector and has rank elements.
  TF_RETURN_IF_ERROR(c->WithRank(perm_shape, 1, &perm_shape));
  TF_RETURN_IF_ERROR(c->WithValue(perm_elems, rank, &perm_elems));

  // If we know the rank of the input and the value of perm, we can return
  // all shape information, otherwise we can only return rank information,
  // but no information for the dimensions.
  if (perm != nullptr) {
    std::vector<int64_t> data;
    if (perm->dtype() == DT_INT32) {
      data = AsInt64<int32>(perm, rank);
    } else {
      data = AsInt64<int64_t>(perm, rank);
    }

    for (int32_t i = 0; i < rank; ++i) {
      int64_t in_idx = data[i];
      if (in_idx >= rank) {
        return errors::InvalidArgument("perm dim ", in_idx,
                                       " is out of range of input rank ", rank);
      }
      dims[i] = c->Dim(input, in_idx);
    }
  } else {
    for (int i = 0; i < rank; ++i) {
      dims[i] = c->UnknownDim();
    }
  }

  c->set_output(0, c->MakeShape(dims));
  return Status::OK();
}