int enc_untrusted_inet_pton(int af, const char *src, void *dst) {
  if (!src || !dst) {
    return 0;
  }

  MessageWriter input;
  input.Push<int>(TokLinuxAfFamily(af));
  input.PushByReference(Extent{
      src, std::min(strlen(src) + 1, static_cast<size_t>(INET6_ADDRSTRLEN))});
  MessageReader output;

  const auto status = NonSystemCallDispatcher(
      ::asylo::host_call::kInetPtonHandler, &input, &output);
  CheckStatusAndParamCount(status, output, "enc_untrusted_inet_pton", 3);

  int result = output.next<int>();
  int klinux_errno = output.next<int>();
  if (result == -1) {
    errno = FromkLinuxErrorNumber(klinux_errno);
    return -1;
  }

  auto klinux_addr_buffer = output.next();
  size_t max_size = 0;
  if (af == AF_INET) {
    max_size = sizeof(struct in_addr);
  } else if (af == AF_INET6) {
    max_size = sizeof(struct in6_addr);
  }
  memcpy(dst, klinux_addr_buffer.data(),
         std::min(klinux_addr_buffer.size(), max_size));
  return result;
}