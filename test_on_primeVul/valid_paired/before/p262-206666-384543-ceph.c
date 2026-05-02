  size_t recv_body(char* buf, size_t max) override {
    auto& message = parser.get();
    auto& body_remaining = message.body();
    body_remaining.data = buf;
    body_remaining.size = max;

    while (body_remaining.size && !parser.is_done()) {
      boost::system::error_code ec;
      http::async_read_some(stream, buffer, parser, yield[ec]);
      if (ec == http::error::partial_message ||
          ec == http::error::need_buffer) {
        break;
      }
      if (ec) {
        ldout(cct, 4) << "failed to read body: " << ec.message() << dendl;
        throw rgw::io::Exception(ec.value(), std::system_category());
      }
    }
    return max - body_remaining.size;
  }