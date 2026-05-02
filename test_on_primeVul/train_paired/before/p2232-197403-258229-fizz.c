folly::Optional<TLSMessage> PlaintextReadRecordLayer::read(
    folly::IOBufQueue& buf) {
  while (true) {
    folly::io::Cursor cursor(buf.front());

    if (buf.empty() || !cursor.canAdvance(kPlaintextHeaderSize)) {
      return folly::none;
    }

    TLSMessage msg;
    msg.type = static_cast<ContentType>(cursor.readBE<ContentTypeType>());

    if (skipEncryptedRecords_) {
      if (msg.type == ContentType::application_data) {
        cursor.skip(sizeof(ProtocolVersion));
        auto length = cursor.readBE<uint16_t>();
        if (buf.chainLength() < (cursor - buf.front()) + length) {
          return folly::none;
        }
        length +=
            sizeof(ContentType) + sizeof(ProtocolVersion) + sizeof(uint16_t);
        buf.trimStart(length);
        continue;
      } else if (msg.type != ContentType::change_cipher_spec) {
        skipEncryptedRecords_ = false;
      }
    }

    switch (msg.type) {
      case ContentType::handshake:
      case ContentType::alert:
        break;
      case ContentType::change_cipher_spec:
        break;
      default:
        throw std::runtime_error(folly::to<std::string>(
            "received plaintext content type ",
            static_cast<ContentTypeType>(msg.type),
            ", header: ",
            folly::hexlify(buf.splitAtMost(10)->coalesce())));
    }

    receivedRecordVersion_ =
        static_cast<ProtocolVersion>(cursor.readBE<ProtocolVersionType>());

    auto length = cursor.readBE<uint16_t>();
    if (length > kMaxPlaintextRecordSize) {
      throw std::runtime_error("received too long plaintext record");
    }
    if (length == 0) {
      throw std::runtime_error("received empty plaintext record");
    }
    if (buf.chainLength() < (cursor - buf.front()) + length) {
      return folly::none;
    }

    cursor.clone(msg.fragment, length);

    buf.trimStart(cursor - buf.front());

    if (msg.type == ContentType::change_cipher_spec) {
      msg.fragment->coalesce();
      if (msg.fragment->length() == 1 && *msg.fragment->data() == 0x01) {
        continue;
      } else {
        throw FizzException(
            "received ccs", AlertDescription::illegal_parameter);
      }
    }

    return std::move(msg);
  }
}