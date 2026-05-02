Status SparseCountSparseOutputShapeFn(InferenceContext *c) {
  ShapeHandle unused;
  TF_RETURN_IF_ERROR(c->WithRank(c->input(0), 2, &unused));
  auto rank = c->Dim(c->input(0), 1);
  auto nvals = c->UnknownDim();
  c->set_output(0, c->Matrix(nvals, rank));  // out.indices
  c->set_output(1, c->Vector(nvals));        // out.values
  c->set_output(2, c->Vector(rank));         // out.dense_shape
  return Status::OK();
}