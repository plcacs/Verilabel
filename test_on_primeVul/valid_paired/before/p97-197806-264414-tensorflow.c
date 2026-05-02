ImmutableConstantOp::ImmutableConstantOp(OpKernelConstruction* context)
    : OpKernel(context) {
  OP_REQUIRES_OK(context,
                 context->GetAttr(kMemoryRegionNameAttr, &region_name_));
  OP_REQUIRES_OK(context, context->GetAttr(kDTypeAttr, &dtype_));
  OP_REQUIRES_OK(context, context->GetAttr(kShapeAttr, &shape_));
}