int64_t LineBasedFrameDecoder::findEndOfLine(IOBufQueue& buf) {
  Cursor c(buf.front());
  for (uint32_t i = 0; i < maxLength_ && i < buf.chainLength(); i++) {
    auto b = c.read<char>();
    if (b == '\n' && terminatorType_ != TerminatorType::CARRIAGENEWLINE) {
      return i;
    } else if (terminatorType_ != TerminatorType::NEWLINE &&
               b == '\r' && !c.isAtEnd() && c.read<char>() == '\n') {
      return i;
    }
  }

  return -1;
}