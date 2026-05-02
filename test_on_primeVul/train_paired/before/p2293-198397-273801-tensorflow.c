TEST(ArrayOpsTest, QuantizeAndDequantizeV2_ShapeFn) {
  ShapeInferenceTestOp op("QuantizeAndDequantizeV2");
  op.input_tensors.resize(3);
  TF_ASSERT_OK(NodeDefBuilder("test", "QuantizeAndDequantizeV2")
                   .Input("input", 0, DT_FLOAT)
                   .Input("input_min", 1, DT_FLOAT)
                   .Input("input_max", 2, DT_FLOAT)
                   .Attr("signed_input", true)
                   .Attr("num_bits", 8)
                   .Attr("range_given", false)
                   .Attr("narrow_range", false)
                   .Attr("axis", -1)
                   .Finalize(&op.node_def));
  INFER_OK(op, "?;?;?", "in0");
  INFER_OK(op, "[];?;?", "in0");
  INFER_OK(op, "[1,2,?,4,5];?;?", "in0");

  INFER_ERROR("Shape must be rank 0 but is rank 1", op, "[1,2,?,4,5];[1];[]");
  INFER_ERROR("Shapes must be equal rank, but are 1 and 0", op,
              "[1,2,?,4,5];[];[1]");
  INFER_ERROR("Shape must be rank 0 but is rank 1", op, "[1,2,?,4,5];[1];[1]");
}