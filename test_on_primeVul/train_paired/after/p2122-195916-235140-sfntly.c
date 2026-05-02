int32_t ByteArray::Get(int32_t index) {
  if (index < 0 || index >= Length())
    return -1;
  return InternalGet(index) & 0xff;
}