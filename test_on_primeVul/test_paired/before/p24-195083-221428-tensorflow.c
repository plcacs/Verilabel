bool Tensor::FromProto(Allocator* a, const TensorProto& proto) {
  CHECK_NOTNULL(a);
  TensorBuffer* p = nullptr;
  if (!TensorShape::IsValid(proto.tensor_shape())) return false;
  if (proto.dtype() == DT_INVALID) return false;
  TensorShape shape(proto.tensor_shape());
  const int64_t N = shape.num_elements();
  if (N > 0 && proto.dtype()) {
    bool dtype_error = false;
    if (!proto.tensor_content().empty()) {
      const auto& content = proto.tensor_content();
      CASES_WITH_DEFAULT(proto.dtype(), p = Helper<T>::Decode(a, content, N),
                         dtype_error = true, dtype_error = true);
    } else {
      CASES_WITH_DEFAULT(proto.dtype(), p = FromProtoField<T>(a, proto, N),
                         dtype_error = true, dtype_error = true);
    }
    if (dtype_error || p == nullptr) return false;
  }
  shape_ = shape;
  set_dtype(proto.dtype());
  UnrefIfNonNull(buf_);
  buf_ = p;
  // TODO(misard) add tracking of which kernels and steps are calling
  // FromProto.
  if (MemoryLoggingEnabled() && buf_ != nullptr && buf_->data() != nullptr) {
    LogMemory::RecordTensorAllocation("Unknown (from Proto)",
                                      LogMemory::UNKNOWN_STEP_ID, *this);
  }
  return true;
}