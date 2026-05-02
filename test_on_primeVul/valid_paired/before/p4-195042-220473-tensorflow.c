TfLiteStatus EvalFloat(TfLiteContext* context, TfLiteNode* node,
                       TfLiteFullyConnectedParams* params, OpData* data,
                       const TfLiteTensor* input, const TfLiteTensor* filter,
                       const TfLiteTensor* bias, TfLiteTensor* output) {
  float output_activation_min, output_activation_max;
  CalculateActivationRange(params->activation, &output_activation_min,
                           &output_activation_max);
  if (kernel_type == kReference) {
    FullyConnectedParams op_params;
    op_params.float_activation_min = output_activation_min;
    op_params.float_activation_max = output_activation_max;
    if (filter->sparsity != nullptr) {
      const auto& sparsity = *filter->sparsity;
      reference_ops::FullyConnectedSparseWeight(
          sparsity, op_params, GetTensorShape(input),
          GetTensorData<float>(input), GetTensorShape(filter),
          GetTensorData<float>(filter), GetTensorShape(bias),
          GetTensorData<float>(bias), GetTensorShape(output),
          GetTensorData<float>(output));
    } else {
      reference_ops::FullyConnected(
          op_params, GetTensorShape(input), GetTensorData<float>(input),
          GetTensorShape(filter), GetTensorData<float>(filter),
          GetTensorShape(bias), GetTensorData<float>(bias),
          GetTensorShape(output), GetTensorData<float>(output));
    }
  } else if (kernel_type == kLegacyPie) {
    return EvalPie(context, node, params, data, input, filter, bias, output);
  } else {
    FullyConnectedParams op_params;
    op_params.float_activation_min = output_activation_min;
    op_params.float_activation_max = output_activation_max;
    if (filter->sparsity != nullptr) {
      const auto& sparsity = *filter->sparsity;
      if (!SupportedSparsityFormat(sparsity)) {
        TF_LITE_KERNEL_LOG(context,
                           "Unsupported sparse fully-connected weight format.");
        return kTfLiteError;
      }

      if (sparsity.dim_metadata_size == kDimMetadataSizeRandomSparse) {
        // Random sparse.
        optimized_ops::FullyConnectedSparseWeight(
            sparsity, op_params, GetTensorShape(input),
            GetTensorData<float>(input), GetTensorShape(filter),
            GetTensorData<float>(filter), GetTensorShape(bias),
            GetTensorData<float>(bias), GetTensorShape(output),
            GetTensorData<float>(output));
      } else if (sparsity.dim_metadata_size == kDimMetadataSizeBlockSparse &&
                 sparsity.dim_metadata[2].dense_size == 4) {
        // Block sparse with block size of 1x4.
        optimized_ops::FullyConnectedSparseWeight1x4(
            sparsity, op_params, GetTensorShape(input),
            GetTensorData<float>(input), GetTensorShape(filter),
            GetTensorData<float>(filter), GetTensorShape(bias),
            GetTensorData<float>(bias), GetTensorShape(output),
            GetTensorData<float>(output),
            CpuBackendContext::GetFromContext(context));
      } else {
        TF_LITE_KERNEL_LOG(context,
                           "Unsupported sparse fully-connected weight format.");
        return kTfLiteError;
      }

    } else {
      op_params.lhs_cacheable = IsConstantTensor(filter);
      op_params.rhs_cacheable = IsConstantTensor(input);
      optimized_ops::FullyConnected(
          op_params, GetTensorShape(input), GetTensorData<float>(input),
          GetTensorShape(filter), GetTensorData<float>(filter),
          GetTensorShape(bias), GetTensorData<float>(bias),
          GetTensorShape(output), GetTensorData<float>(output),
          CpuBackendContext::GetFromContext(context));
    }
  }

  return kTfLiteOk;
}