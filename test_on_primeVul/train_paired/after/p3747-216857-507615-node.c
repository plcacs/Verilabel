void Http2Stream::SubmitRstStream(const uint32_t code) {
  CHECK(!this->is_destroyed());
  code_ = code;

  auto is_stream_cancel = [](const uint32_t code) {
    return code == NGHTTP2_CANCEL;
  };

  // If RST_STREAM frame is received with error code NGHTTP2_CANCEL,
  // add it to the pending list and don't force purge the data. It is
  // to avoids the double free error due to unwanted behavior of nghttp2.

  // Add stream to the pending list only if it is received with scope
  // below in the stack. The pending list may not get processed
  // if RST_STREAM received is not in scope and added to the list
  // causing endpoint to hang.
  if (session_->is_in_scope() && is_stream_cancel(code)) {
      session_->AddPendingRstStream(id_);
      return;
  }


  // If possible, force a purge of any currently pending data here to make sure
  // it is sent before closing the stream. If it returns non-zero then we need
  // to wait until the current write finishes and try again to avoid nghttp2
  // behaviour where it prioritizes RstStream over everything else.
  if (session_->SendPendingData() != 0) {
    session_->AddPendingRstStream(id_);
    return;
  }

  FlushRstStream();
}