int ssh_buffer_add_data(struct ssh_buffer_struct *buffer, const void *data, uint32_t len)
{
    buffer_verify(buffer);

    if (data == NULL) {
        return -1;
    }

    if (buffer->used + len < len) {
        return -1;
    }

    if (buffer->allocated < (buffer->used + len)) {
        if (buffer->pos > 0) {
            buffer_shift(buffer);
        }
        if (realloc_buffer(buffer, buffer->used + len) < 0) {
            return -1;
        }
    }

    memcpy(buffer->data + buffer->used, data, len);
    buffer->used += len;
    buffer_verify(buffer);
    return 0;
}