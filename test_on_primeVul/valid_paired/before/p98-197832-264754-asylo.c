void *UntrustedCacheMalloc::GetBuffer() {
  void **buffers = nullptr;
  void *buffer;
  bool is_pool_empty;

  {
    LockGuard spin_lock(&lock_);
    is_pool_empty = buffer_pool_.empty();
    if (is_pool_empty) {
      buffers =
          primitives::AllocateUntrustedBuffers(kPoolIncrement, kPoolEntrySize);
      for (int i = 0; i < kPoolIncrement; i++) {
        if (!buffers[i] ||
            !TrustedPrimitives::IsOutsideEnclave(buffers[i], kPoolEntrySize)) {
          abort();
        }
        buffer_pool_.push(buffers[i]);
      }
    }
    buffer = buffer_pool_.top();
    buffer_pool_.pop();
    busy_buffers_.insert(buffer);
  }

  if (is_pool_empty) {
    // Free memory held by the array of buffer pointers returned by
    // AllocateUntrustedBuffers.
    Free(buffers);
  }
  return buffer;
}