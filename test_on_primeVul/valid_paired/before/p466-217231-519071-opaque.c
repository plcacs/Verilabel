void ocall_malloc(size_t size, uint8_t **ret) {
  *ret = static_cast<uint8_t *>(malloc(size));
}