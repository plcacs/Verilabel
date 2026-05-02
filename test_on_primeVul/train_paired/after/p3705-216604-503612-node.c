inline void* Zone::New(int size) {
  ASSERT(scope_nesting_ > 0);
  // Round up the requested size to fit the alignment.
  size = RoundUp(size, kAlignment);

  // If the allocation size is divisible by 8 then we return an 8-byte aligned
  // address.
  if (kPointerSize == 4 && kAlignment == 4) {
    position_ += ((~size) & 4) & (reinterpret_cast<intptr_t>(position_) & 4);
  } else {
    ASSERT(kAlignment >= kPointerSize);
  }

  // Check if the requested size is available without expanding.
  Address result = position_;

  const uintptr_t limit = reinterpret_cast<uintptr_t>(limit_);
  const uintptr_t position = reinterpret_cast<uintptr_t>(position_);
  // position_ > limit_ can be true after the alignment correction above.
  if (limit < position || size > limit - position) {
     result = NewExpand(size);
  } else {
     position_ += size;
  }

  // Check that the result has the proper alignment and return it.
  ASSERT(IsAddressAligned(result, kAlignment, 0));
  allocation_size_ += size;
  return reinterpret_cast<void*>(result);
}