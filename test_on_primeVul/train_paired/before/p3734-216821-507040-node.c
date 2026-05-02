int LibuvStreamWrap::DoWrite(WriteWrap* req_wrap,
                             uv_buf_t* bufs,
                             size_t count,
                             uv_stream_t* send_handle) {
  LibuvWriteWrap* w = static_cast<LibuvWriteWrap*>(req_wrap);
  int r;
  if (send_handle == nullptr) {
    r = w->Dispatch(uv_write, stream(), bufs, count, AfterUvWrite);
  } else {
    r = w->Dispatch(uv_write2,
                    stream(),
                    bufs,
                    count,
                    send_handle,
                    AfterUvWrite);
  }

  if (!r) {
    size_t bytes = 0;
    for (size_t i = 0; i < count; i++)
      bytes += bufs[i].len;
    if (stream()->type == UV_TCP) {
      NODE_COUNT_NET_BYTES_SENT(bytes);
    } else if (stream()->type == UV_NAMED_PIPE) {
      NODE_COUNT_PIPE_BYTES_SENT(bytes);
    }
  }

  return r;
}