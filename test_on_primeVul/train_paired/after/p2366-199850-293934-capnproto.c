inline bool SegmentReader::containsInterval(const void* from, const void* to) {
  uintptr_t start = reinterpret_cast<uintptr_t>(from) - reinterpret_cast<uintptr_t>(ptr.begin());
  uintptr_t end = reinterpret_cast<uintptr_t>(to) - reinterpret_cast<uintptr_t>(ptr.begin());
  uintptr_t bound = ptr.size() * sizeof(capnp::word);

  return start <= bound && end <= bound && start <= end &&
      readLimiter->canRead(
          intervalLength(reinterpret_cast<const byte*>(from),
                         reinterpret_cast<const byte*>(to),
                         MAX_SEGMENT_WORDS * BYTES_PER_WORD)
              / BYTES_PER_WORD,
          arena);
}