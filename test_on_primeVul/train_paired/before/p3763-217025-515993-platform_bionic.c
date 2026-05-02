void* chk_malloc(size_t bytes)
{
    char* buffer = (char*)dlmalloc(bytes + CHK_OVERHEAD_SIZE);
    if (buffer) {
        memset(buffer, CHK_SENTINEL_VALUE, bytes + CHK_OVERHEAD_SIZE);
        size_t offset = dlmalloc_usable_size(buffer) - sizeof(size_t);
        *(size_t *)(buffer + offset) = bytes;
        buffer += CHK_SENTINEL_HEAD_SIZE;
    }
    return buffer;
}