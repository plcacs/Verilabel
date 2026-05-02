inline bool SegmentReader::containsInterval(const void* from, const void* to) {
  return from >= this->ptr.begin() && to <= this->ptr.end() && from <= to &&
      readLimiter->canRead(
          intervalLength(reinterpret_cast<const byte*>(from),
                         reinterpret_cast<const byte*>(to),
                         MAX_SEGMENT_WORDS * BYTES_PER_WORD)
              / BYTES_PER_WORD,
          arena);
}