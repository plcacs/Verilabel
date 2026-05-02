int32_t *enc_untrusted_create_wait_queue() {
  MessageWriter input;
  MessageReader output;
  input.Push<uint64_t>(sizeof(int32_t));
  const auto status = NonSystemCallDispatcher(
      ::asylo::host_call::kLocalLifetimeAllocHandler, &input, &output);
  CheckStatusAndParamCount(status, output, "enc_untrusted_create_wait_queue",
                           2);
  int32_t *queue = reinterpret_cast<int32_t *>(output.next<uintptr_t>());
  int klinux_errno = output.next<int>();
  if (queue == nullptr) {
    errno = FromkLinuxErrorNumber(klinux_errno);
  }
  enc_untrusted_disable_waiting(queue);
  return queue;
}