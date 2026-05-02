folly::Optional<ErrorCode> HTTP2Codec::parseHeadersDecodeFrames(
    const folly::Optional<http2::PriorityUpdate>& priority,
    const folly::Optional<uint32_t>& promisedStream,
    const folly::Optional<ExAttributes>& exAttributes,
    std::unique_ptr<HTTPMessage>& msg) {
  // decompress headers
  Cursor headerCursor(curHeaderBlock_.front());
  bool isReq = false;
  if (promisedStream) {
    isReq = true;
  } else if (exAttributes) {
    isReq = isRequest(curHeader_.stream);
  } else {
    isReq = transportDirection_ == TransportDirection::DOWNSTREAM;
  }

  decodeInfo_.init(isReq, parsingDownstreamTrailers_);
  if (priority) {
    if (curHeader_.stream == priority->streamDependency) {
      streamError(folly::to<string>("Circular dependency for txn=",
                                    curHeader_.stream),
                  ErrorCode::PROTOCOL_ERROR,
                  curHeader_.type == http2::FrameType::HEADERS);
      return ErrorCode::NO_ERROR;
    }

    decodeInfo_.msg->setHTTP2Priority(
        std::make_tuple(priority->streamDependency,
                        priority->exclusive,
                        priority->weight));
  }
  headerCodec_.decodeStreaming(
      headerCursor, curHeaderBlock_.chainLength(), this);
  msg = std::move(decodeInfo_.msg);
  // Saving this in case we need to log it on error
  auto g = folly::makeGuard([this] { curHeaderBlock_.move(); });
  // Check decoding error
  if (decodeInfo_.decodeError != HPACK::DecodeError::NONE) {
    static const std::string decodeErrorMessage =
        "Failed decoding header block for stream=";
    // Avoid logging header blocks that have failed decoding due to being
    // excessively large.
    if (decodeInfo_.decodeError != HPACK::DecodeError::HEADERS_TOO_LARGE) {
      LOG(ERROR) << decodeErrorMessage << curHeader_.stream
                 << " header block=";
      VLOG(3) << IOBufPrinter::printHexFolly(curHeaderBlock_.front(), true);
    } else {
      LOG(ERROR) << decodeErrorMessage << curHeader_.stream;
    }

    if (msg) {
      // print the partial message
      msg->dumpMessage(3);
    }
    return ErrorCode::COMPRESSION_ERROR;
  }

  // Check parsing error
  if (decodeInfo_.parsingError != "") {
    LOG(ERROR) << "Failed parsing header list for stream=" << curHeader_.stream
               << ", error=" << decodeInfo_.parsingError << ", header block=";
    VLOG(3) << IOBufPrinter::printHexFolly(curHeaderBlock_.front(), true);
    HTTPException err(HTTPException::Direction::INGRESS,
                      folly::to<std::string>("HTTP2Codec stream error: ",
                                             "stream=",
                                             curHeader_.stream,
                                             " status=",
                                             400,
                                             " error: ",
                                             decodeInfo_.parsingError));
    err.setHttpStatusCode(400);
    callback_->onError(curHeader_.stream, err, true);
    return ErrorCode::NO_ERROR;
  }

  return folly::Optional<ErrorCode>();
}