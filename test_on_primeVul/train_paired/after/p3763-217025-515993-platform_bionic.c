void* chk_malloc(size_t bytes)
{
    size_t size = bytes + CHK_OVERHEAD_SIZE;
    if (size < bytes) { // Overflow.
        return NULL;
    }
    uint8_t* buffer = (uint8_t*) dlmalloc(size);
    if (buffer) {
        memset(buffer, CHK_SENTINEL_VALUE, bytes + CHK_OVERHEAD_SIZE);
        size_t offset = dlmalloc_usable_size(buffer) - sizeof(size_t);
        *(size_t *)(buffer + offset) = bytes;
        buffer += CHK_SENTINEL_HEAD_SIZE;
    }
    return buffer;
}