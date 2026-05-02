void *ssh_buffer_allocate(struct ssh_buffer_struct *buffer, uint32_t len)
{
    void *ptr;
    buffer_verify(buffer);

    if (buffer->used + len < len) {
        return NULL;
    }

    if (buffer->allocated < (buffer->used + len)) {
        if (buffer->pos > 0) {
            buffer_shift(buffer);
        }

        if (realloc_buffer(buffer, buffer->used + len) < 0) {
            return NULL;
        }
    }

    ptr = buffer->data + buffer->used;
    buffer->used+=len;
    buffer_verify(buffer);

    return ptr;
}