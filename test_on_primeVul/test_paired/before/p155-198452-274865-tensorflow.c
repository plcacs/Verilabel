void ComparisonQuantized(const TfLiteTensor* input1, const TfLiteTensor* input2,
                         TfLiteTensor* output, bool requires_broadcast) {
  if (input1->type == kTfLiteUInt8 || input1->type == kTfLiteInt8) {
    auto input1_offset = -input1->params.zero_point;
    auto input2_offset = -input2->params.zero_point;
    const int left_shift = 8;

    int32 input1_multiplier;
    int input1_shift;
    QuantizeMultiplierSmallerThanOneExp(input1->params.scale,
                                        &input1_multiplier, &input1_shift);
    int32 input2_multiplier;
    int input2_shift;
    QuantizeMultiplierSmallerThanOneExp(input2->params.scale,
                                        &input2_multiplier, &input2_shift);

    ComparisonParams op_params;
    op_params.left_shift = left_shift;
    op_params.input1_offset = input1_offset;
    op_params.input1_multiplier = input1_multiplier;
    op_params.input1_shift = input1_shift;
    op_params.input2_offset = input2_offset;
    op_params.input2_multiplier = input2_multiplier;
    op_params.input2_shift = input2_shift;
    if (requires_broadcast) {
      reference_ops::BroadcastComparison4DSlowWithScaling<input_dtype, opname>(
          op_params, GetTensorShape(input1), GetTensorData<input_dtype>(input1),
          GetTensorShape(input2), GetTensorData<input_dtype>(input2),
          GetTensorShape(output), GetTensorData<bool>(output));
    } else {
      reference_ops::ComparisonWithScaling<input_dtype, opname>(
          op_params, GetTensorShape(input1), GetTensorData<input_dtype>(input1),
          GetTensorShape(input2), GetTensorData<input_dtype>(input2),
          GetTensorShape(output), GetTensorData<bool>(output));
    }
  }
}