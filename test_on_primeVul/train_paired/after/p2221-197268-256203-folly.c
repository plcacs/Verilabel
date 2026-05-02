BufferedRandomDevice::BufferedRandomDevice(size_t bufferSize)
  : bufferSize_(bufferSize),
    buffer_(new unsigned char[bufferSize]),
    ptr_(buffer_.get() + bufferSize) {  // refill on first use
  call_once(flag, [this]() {
    detail::AtFork::registerHandler(
        this,
        /*prepare*/ []() { return true; },
        /*parent*/ []() {},
        /*child*/
        []() {
          using Single = SingletonThreadLocal<BufferedRandomDevice, RandomTag>;
          auto& t = Single::get();
          // Clear out buffered data on fork.
          //
          // Ensure child and parent do not share same entropy pool.
          t.ptr_ = t.buffer_.get() + t.bufferSize_;
        });
  });
}