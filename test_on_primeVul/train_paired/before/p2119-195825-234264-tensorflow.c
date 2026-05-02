TfLiteStatus ResizeOutputTensors(TfLiteContext* context, TfLiteNode* node,
                                 const TfLiteTensor* input,
                                 const TfLiteTensor* size_splits,
                                 const TfLiteTensor* axis) {
  int axis_value = GetTensorData<int>(axis)[0];
  if (axis_value < 0) {
    axis_value += NumDimensions(input);
  }

  std::vector<int64_t> size_splits_vector;
  if (size_splits->type == kTfLiteInt32) {
    GetSizeSplitsVector<int32_t>(size_splits, &size_splits_vector);
  } else if (size_splits->type == kTfLiteInt64) {
    GetSizeSplitsVector<int64_t>(size_splits, &size_splits_vector);
  } else {
    context->ReportError(context, "size_splits only support type int32|int64.");
    return kTfLiteError;
  }

  int minus_one_index = -1;
  int64_t size_splits_sum = 0;

  for (int i = 0; i < size_splits_vector.size(); ++i) {
    if (size_splits_vector.at(i) == -1) {
      if (minus_one_index == -1) {
        minus_one_index = i;
      } else {
        context->ReportError(context,
                             "The size_splits contains more than one -1.");
      }
    } else {
      size_splits_sum += size_splits_vector.at(i);
    }
  }

  const int input_size = SizeOfDimension(input, axis_value);

  if (minus_one_index != -1) {
    if (size_splits_sum > input_size) {
      context->ReportError(
          context,
          "The sum of size_splits must be less than the dimension of value.");
    } else {
      size_splits_vector[minus_one_index] = input_size - size_splits_sum;
    }
  } else if (size_splits_sum != input_size) {
    context->ReportError(
        context,
        "The size_splits must sum to the dimension of value along axis.");
  }

  for (int i = 0; i < NumOutputs(node); ++i) {
    TfLiteIntArray* output_dims = TfLiteIntArrayCopy(input->dims);
    output_dims->data[axis_value] = size_splits_vector.at(i);
    TfLiteTensor* output;
    TF_LITE_ENSURE_OK(context, GetOutputSafe(context, node, i, &output));
    TF_LITE_ENSURE_STATUS(context->ResizeTensor(context, output, output_dims));
  }

  return kTfLiteOk;
}