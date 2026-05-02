ssize_t enc_untrusted_read(int fd, void *buf, size_t count) {
  ssize_t ret = static_cast<ssize_t>(EnsureInitializedAndDispatchSyscall(
      asylo::system_call::kSYS_read, fd, buf, count));
  if (ret != -1 && ret > count) {
    ::asylo::primitives::TrustedPrimitives::BestEffortAbort(
        "enc_untrusted_read: read result exceeds requested");
  }
  return ret;
}