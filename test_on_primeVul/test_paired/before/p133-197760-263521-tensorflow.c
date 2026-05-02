TfLiteStatus EvalGatherNd(TfLiteContext* context, const TfLiteTensor* params,
                          const TfLiteTensor* indices, TfLiteTensor* output) {
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