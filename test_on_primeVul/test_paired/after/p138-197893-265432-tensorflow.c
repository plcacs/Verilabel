TfLiteStatus Gather(TfLiteContext* context, const TfLiteGatherParams& params,
                    const TfLiteTensor* input, const TfLiteTensor* positions,
                    TfLiteTensor* output) {
  const PositionsT* indexes = GetTensorData<PositionsT>(positions);
  bool indices_has_only_positive_elements = true;
  const size_t num_indices = positions->bytes / sizeof(PositionsT);
  for (size_t i = 0; i < num_indices; i++) {
    if (indexes[i] < 0) {
      indices_has_only_positive_elements = false;
      break;
    }
  }
  TF_LITE_ENSURE(context, indices_has_only_positive_elements);

  tflite::GatherParams op_params;
  op_params.axis = params.axis;
  op_params.batch_dims = params.batch_dims;
  optimized_ops::Gather(op_params, GetTensorShape(input),
                        GetTensorData<InputT>(input), GetTensorShape(positions),
                        GetTensorData<PositionsT>(positions),
                        GetTensorShape(output), GetTensorData<InputT>(output));
  return kTfLiteOk;
}