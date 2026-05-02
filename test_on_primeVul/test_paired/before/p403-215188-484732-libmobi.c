void mobi_buffer_move(MOBIBuffer *buf, const int offset, const size_t len) {
    size_t aoffset = (size_t) abs(offset);
    unsigned char *source = buf->data + buf->offset;
    if (offset >= 0) {
        if (buf->offset + aoffset + len > buf->maxlen) {
            debug_print("%s", "End of buffer\n");
            buf->error = MOBI_BUFFER_END;
            return;
        }
        source += aoffset;
    } else {
        if (buf->offset < aoffset) {
            debug_print("%s", "End of buffer\n");
            buf->error = MOBI_BUFFER_END;
            return;
        }
        source -= aoffset;
    }
    memmove(buf->data + buf->offset, source, len);
    buf->offset += len;
}