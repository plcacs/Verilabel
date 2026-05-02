TfLiteStatus EvalGatherNd(TfLiteContext* context, const TfLiteTensor* params,
                          const TfLiteTensor* indices, TfLiteTensor* output) {
  bool indices_has_only_positive_elements = true;
  const auto* indices_values = GetTensorData<IndicesT>(indices);
  const size_t num_indices = indices->bytes / sizeof(IndicesT);
  for (size_t i = 0; i < num_indices; i++) {
    if (indices_values[i] < 0) {
      indices_has_only_positive_elements = false;
      break;
    }
  }
  TF_LITE_ENSURE(context, indices_has_only_positive_elements);

  switch (params->type) {
    case kTfLiteFloat32:
      return GatherNd<float, IndicesT>(params, indices, output);
    case kTfLiteUInt8:
      return GatherNd<uint8_t, IndicesT>(params, indices, output);
    case kTfLiteInt8:
      return GatherNd<int8_t, IndicesT>(params, indices, output);
    case kTfLiteInt16:
      return GatherNd<int16_t, IndicesT>(params, indices, output);
    case kTfLiteInt32:
      return GatherNd<int32_t, IndicesT>(params, indices, output);
    case kTfLiteInt64:
      return GatherNd<int64_t, IndicesT>(params, indices, output);
    case kTfLiteString:
      return GatherNdString<IndicesT>(params, indices, output);
    default:
      context->ReportError(context,
                           "Params type '%s' are not supported by gather_nd.",
                           TfLiteTypeGetName(params->type));
      return kTfLiteError;
  }
}