void Http2Stream::SubmitRstStream(const uint32_t code) {
  CHECK(!this->is_destroyed());
  code_ = code;

  // If RST_STREAM frame is received and stream is not writable
  // because it is busy reading data, don't try force purging it.
  // Instead add the stream to pending stream list and process
  // the pending data when it is safe to do so. This is to avoid
  // double free error due to unwanted behavior of nghttp2.
  // Ref:https://github.com/nodejs/node/issues/38964

  // Add stream to the pending list if it is received with scope
  // below in the stack. The pending list may not get processed
  // if RST_STREAM received is not in scope and added to the list
  // causing endpoint to hang.
  if (session_->is_in_scope() &&
      !is_writable() && is_reading()) {
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