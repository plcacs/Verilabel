int TLSWrap::DoWrite(WriteWrap* w,
                     uv_buf_t* bufs,
                     size_t count,
                     uv_stream_t* send_handle) {
  CHECK_NULL(send_handle);
  Debug(this, "DoWrite()");

  if (ssl_ == nullptr) {
    ClearError();
    error_ = "Write after DestroySSL";
    return UV_EPROTO;
  }

  size_t length = 0;
  size_t i;
  for (i = 0; i < count; i++)
    length += bufs[i].len;

  // We want to trigger a Write() on the underlying stream to drive the stream
  // system, but don't want to encrypt empty buffers into a TLS frame, so see
  // if we can find something to Write().
  // First, call ClearOut(). It does an SSL_read(), which might cause handshake
  // or other internal messages to be encrypted. If it does, write them later
  // with EncOut().
  // If there is still no encrypted output, call Write(bufs) on the underlying
  // stream. Since the bufs are empty, it won't actually write non-TLS data
  // onto the socket, we just want the side-effects. After, make sure the
  // WriteWrap was accepted by the stream, or that we call Done() on it.
  if (length == 0) {
    Debug(this, "Empty write");
    ClearOut();
    if (BIO_pending(enc_out_) == 0) {
      Debug(this, "No pending encrypted output, writing to underlying stream");
      CHECK_NULL(current_empty_write_);
      current_empty_write_ = w;
      StreamWriteResult res =
          underlying_stream()->Write(bufs, count, send_handle);
      if (!res.async) {
        env()->SetImmediate([](Environment* env, void* data) {
          TLSWrap* self = static_cast<TLSWrap*>(data);
          self->OnStreamAfterWrite(self->current_empty_write_, 0);
        }, this, object());
      }
      return 0;
    }
  }

  // Store the current write wrap
  CHECK_NULL(current_write_);
  current_write_ = w;

  // Write encrypted data to underlying stream and call Done().
  if (length == 0) {
    EncOut();
    return 0;
  }

  std::vector<char> data;
  crypto::MarkPopErrorOnReturn mark_pop_error_on_return;

  int written = 0;
  if (count != 1) {
    data = std::vector<char>(length);
    size_t offset = 0;
    for (i = 0; i < count; i++) {
      memcpy(data.data() + offset, bufs[i].base, bufs[i].len);
      offset += bufs[i].len;
    }
    written = SSL_write(ssl_.get(), data.data(), length);
  } else {
    // Only one buffer: try to write directly, only store if it fails
    written = SSL_write(ssl_.get(), bufs[0].base, bufs[0].len);
    if (written == -1) {
      data = std::vector<char>(length);
      memcpy(data.data(), bufs[0].base, bufs[0].len);
    }
  }

  CHECK(written == -1 || written == static_cast<int>(length));
  Debug(this, "Writing %zu bytes, written = %d", length, written);

  if (written == -1) {
    int err;
    Local<Value> arg = GetSSLError(written, &err, &error_);

    // If we stopped writing because of an error, it's fatal, discard the data.
    if (!arg.IsEmpty()) {
      Debug(this, "Got SSL error (%d), returning UV_EPROTO", err);
      current_write_ = nullptr;
      return UV_EPROTO;
    }

    Debug(this, "Saving data for later write");
    // Otherwise, save unwritten data so it can be written later by ClearIn().
    CHECK_EQ(pending_cleartext_input_.size(), 0);
    pending_cleartext_input_ = std::move(data);
  }

  // Write any encrypted/handshake output that may be ready.
  EncOut();

  return 0;
}