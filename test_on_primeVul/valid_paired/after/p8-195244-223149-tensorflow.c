int64_t TensorByteSize(const TensorProto& t) {
  // num_elements returns -1 if shape is not fully defined.
  int64_t num_elems = PartialTensorShape(t.tensor_shape()).num_elements();
  return num_elems < 0 ? -1 : num_elems * DataTypeSize(t.dtype());
}