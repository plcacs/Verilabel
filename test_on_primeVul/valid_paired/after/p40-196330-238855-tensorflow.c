static inline Status ParseAndCheckBoxSizes(const Tensor& boxes,
                                           const Tensor& box_index,
                                           int* num_boxes) {
  if (boxes.NumElements() == 0 && box_index.NumElements() == 0) {
    *num_boxes = 0;
    return Status::OK();
  }
  // The shape of 'boxes' is [num_boxes, 4].
  if (boxes.dims() != 2) {
    return errors::InvalidArgument("boxes must be 2-D",
                                   boxes.shape().DebugString());
  }
  *num_boxes = boxes.dim_size(0);
  if (boxes.dim_size(1) != 4) {
    return errors::InvalidArgument("boxes must have 4 columns");
  }
  for (int64 i = 0; i < *num_boxes; i++) {
    for (int64 j = 0; j < 4; j++) {
      if (!isfinite(boxes.tensor<float, 2>()(i, j))) {
        return errors::InvalidArgument(
            "boxes values must be finite, received boxes[", i, "]: ",
            boxes.tensor<float, 2>()(i, 0), ", ",
            boxes.tensor<float, 2>()(i, 1), ", ",
            boxes.tensor<float, 2>()(i, 2), ", ",
            boxes.tensor<float, 2>()(i, 3));
      }
    }
  }
  // The shape of 'box_index' is [num_boxes].
  if (box_index.dims() != 1) {
    return errors::InvalidArgument("box_index must be 1-D",
                                   box_index.shape().DebugString());
  }
  if (box_index.dim_size(0) != *num_boxes) {
    return errors::InvalidArgument("box_index has incompatible shape");
  }
  return Status::OK();
}