int ecall_restore(const char *input, uint64_t input_len, char **output,
                  uint64_t *output_len) {
  if (!asylo::primitives::TrustedPrimitives::IsOutsideEnclave(input,
                                                              input_len) ||
      !asylo::primitives::TrustedPrimitives::IsOutsideEnclave(
          output_len, sizeof(uint64_t))) {
    asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "ecall_restore: input/output found to not be in untrusted memory.");
  }
  int result = 0;
  size_t tmp_output_len;
  try {
    result = asylo::Restore(input, static_cast<size_t>(input_len), output,
                            &tmp_output_len);
  } catch (...) {
    LOG(FATAL) << "Uncaught exception in enclave";
  }

  if (output_len) {
    *output_len = static_cast<uint64_t>(tmp_output_len);
  }
  return result;
}