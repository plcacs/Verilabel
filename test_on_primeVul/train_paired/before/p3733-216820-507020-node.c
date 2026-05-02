int JSStream::DoWrite(WriteWrap* w,
                      uv_buf_t* bufs,
                      size_t count,
                      uv_stream_t* send_handle) {
  CHECK_NULL(send_handle);

  HandleScope scope(env()->isolate());
  Context::Scope context_scope(env()->context());

  Local<Array> bufs_arr = Array::New(env()->isolate(), count);
  Local<Object> buf;
  for (size_t i = 0; i < count; i++) {
    buf = Buffer::Copy(env(), bufs[i].base, bufs[i].len).ToLocalChecked();
    bufs_arr->Set(i, buf);
  }

  Local<Value> argv[] = {
    w->object(),
    bufs_arr
  };

  TryCatch try_catch(env()->isolate());
  Local<Value> value;
  int value_int = UV_EPROTO;
  if (!MakeCallback(env()->onwrite_string(),
                    arraysize(argv),
                    argv).ToLocal(&value) ||
      !value->Int32Value(env()->context()).To(&value_int)) {
    if (!try_catch.HasTerminated())
      FatalException(env()->isolate(), try_catch);
  }
  return value_int;
}