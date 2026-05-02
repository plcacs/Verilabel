TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  OpContext op_context(context, node);

    switch (op_context.output->type) {
      case kTfLiteFloat32:
        TFLiteOperation<kernel_type, float, OpType>(context, node, op_context);
        break;
      case kTfLiteUInt8:
        TFLiteOperation<kernel_type, uint8_t, OpType>(context, node,
                                                      op_context);
        break;
      case kTfLiteInt8:
        TFLiteOperation<kernel_type, int8_t, OpType>(context, node, op_context);
        break;
      case kTfLiteInt32:
        TFLiteOperation<kernel_type, int32_t, OpType>(context, node,
                                                      op_context);
        break;
      case kTfLiteInt64:
        TFLiteOperation<kernel_type, int64_t, OpType>(context, node,
                                                      op_context);
        break;
      case kTfLiteInt16:
        TFLiteOperation<kernel_type, int16_t, OpType>(context, node,
                                                      op_context);
        break;
      default:
        context->ReportError(context,
                             "Type %d is currently not supported by Maximum.",
                             op_context.output->type);
        return kTfLiteError;
    }
  return kTfLiteOk;
}