bool FromkLinuxSockAddr(const struct klinux_sockaddr *input,
                        socklen_t input_len, struct sockaddr *output,
                        socklen_t *output_len,
                        void (*abort_handler)(const char *)) {
  if (!input || !output || !output_len || input_len == 0) {
    output = nullptr;
    return false;
  }

  int16_t klinux_family = input->klinux_sa_family;
  if (klinux_family == kLinux_AF_UNIX) {
    struct klinux_sockaddr_un *klinux_sockaddr_un_in =
        const_cast<struct klinux_sockaddr_un *>(
            reinterpret_cast<const struct klinux_sockaddr_un *>(input));

    struct sockaddr_un sockaddr_un_out;
    sockaddr_un_out.sun_family = AF_UNIX;
    InitializeToZeroArray(sockaddr_un_out.sun_path);
    ReinterpretCopyArray(
        sockaddr_un_out.sun_path, klinux_sockaddr_un_in->klinux_sun_path,
        std::min(sizeof(sockaddr_un_out.sun_path),
                 sizeof(klinux_sockaddr_un_in->klinux_sun_path)));
    CopySockaddr(&sockaddr_un_out, sizeof(sockaddr_un_out), output, output_len);
  } else if (klinux_family == kLinux_AF_INET) {
    struct klinux_sockaddr_in *klinux_sockaddr_in_in =
        const_cast<struct klinux_sockaddr_in *>(
            reinterpret_cast<const struct klinux_sockaddr_in *>(input));

    struct sockaddr_in sockaddr_in_out;
    sockaddr_in_out.sin_family = AF_INET;
    sockaddr_in_out.sin_port = klinux_sockaddr_in_in->klinux_sin_port;
    InitializeToZeroSingle(&sockaddr_in_out.sin_addr);
    ReinterpretCopySingle(&sockaddr_in_out.sin_addr,
                          &klinux_sockaddr_in_in->klinux_sin_addr);
    InitializeToZeroArray(sockaddr_in_out.sin_zero);
    ReinterpretCopyArray(sockaddr_in_out.sin_zero,
                         klinux_sockaddr_in_in->klinux_sin_zero);
    CopySockaddr(&sockaddr_in_out, sizeof(sockaddr_in_out), output, output_len);
  } else if (klinux_family == kLinux_AF_INET6) {
    struct klinux_sockaddr_in6 *klinux_sockaddr_in6_in =
        const_cast<struct klinux_sockaddr_in6 *>(
            reinterpret_cast<const struct klinux_sockaddr_in6 *>(input));

    struct sockaddr_in6 sockaddr_in6_out;
    sockaddr_in6_out.sin6_family = AF_INET6;
    sockaddr_in6_out.sin6_port = klinux_sockaddr_in6_in->klinux_sin6_port;
    sockaddr_in6_out.sin6_flowinfo =
        klinux_sockaddr_in6_in->klinux_sin6_flowinfo;
    sockaddr_in6_out.sin6_scope_id =
        klinux_sockaddr_in6_in->klinux_sin6_scope_id;
    InitializeToZeroSingle(&sockaddr_in6_out.sin6_addr);
    ReinterpretCopySingle(&sockaddr_in6_out.sin6_addr,
                          &klinux_sockaddr_in6_in->klinux_sin6_addr);
    CopySockaddr(&sockaddr_in6_out, sizeof(sockaddr_in6_out), output,
                 output_len);
  } else if (klinux_family == kLinux_AF_UNSPEC) {
    output = nullptr;
    *output_len = 0;
  } else {
    if (abort_handler != nullptr) {
      std::string message = absl::StrCat(
          "Type conversion error - Unsupported AF family: ", klinux_family);
      abort_handler(message.c_str());
    } else {
      abort();
    }
  }
  return true;
}