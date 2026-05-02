TfLiteStatus ResizeOutput(TfLiteContext* context, const TfLiteTensor* input,
                          const TfLiteTensor* axis, TfLiteTensor* output) {
  int axis_value;
  // Retrive all 8 bytes when axis type is kTfLiteInt64 to avoid data loss.
  if (axis->type == kTfLiteInt64) {
    axis_value = static_cast<int>(*GetTensorData<int64_t>(axis));
  } else {
    axis_value = *GetTensorData<int>(axis);
  }
  if (axis_value < 0) {
    axis_value += NumDimensions(input);
  }

  // Copy the input dimensions to output except the axis dimension.
  TfLiteIntArray* output_dims = TfLiteIntArrayCreate(NumDimensions(input) - 1);
  int j = 0;
  for (int i = 0; i < NumDimensions(input); ++i) {
    if (i != axis_value) {
      output_dims->data[j] = SizeOfDimension(input, i);
      ++j;
    }
  }
  return context->ResizeTensor(context, output, output_dims);
}