int32_t ByteArray::Get(int32_t index) {
  return InternalGet(index) & 0xff;
}