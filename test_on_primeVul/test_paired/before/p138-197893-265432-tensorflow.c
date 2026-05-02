TfLiteStatus Gather(const TfLiteGatherParams& params, const TfLiteTensor* input,
                    const TfLiteTensor* positions, TfLiteTensor* output) {
  tflite::GatherParams op_params;
  op_params.axis = params.axis;
  op_params.batch_dims = params.batch_dims;
  optimized_ops::Gather(op_params, GetTensorShape(input),
                        GetTensorData<InputT>(input), GetTensorShape(positions),
                        GetTensorData<PositionsT>(positions),
                        GetTensorShape(output), GetTensorData<InputT>(output));
  return kTfLiteOk;
}