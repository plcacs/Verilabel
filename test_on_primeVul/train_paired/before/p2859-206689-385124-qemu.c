static size_t oss_write(HWVoiceOut *hw, void *buf, size_t len)
{
    OSSVoiceOut *oss = (OSSVoiceOut *) hw;
    size_t pos;

    if (oss->mmapped) {
        size_t total_len;
        len = MIN(len, oss_get_available_bytes(oss));

        total_len = len;
        while (len) {
            size_t to_copy = MIN(len, hw->size_emul - hw->pos_emul);
            memcpy(hw->buf_emul + hw->pos_emul, buf, to_copy);

            hw->pos_emul = (hw->pos_emul + to_copy) % hw->pos_emul;
            buf += to_copy;
            len -= to_copy;
        }
        return total_len;
    }

    pos = 0;
    while (len) {
        ssize_t bytes_written;
        void *pcm = advance(buf, pos);

        bytes_written = write(oss->fd, pcm, len);
        if (bytes_written < 0) {
            if (errno != EAGAIN) {
                oss_logerr(errno, "failed to write %zu bytes\n",
                           len);
            }
            return pos;
        }

        pos += bytes_written;
        if (bytes_written < len) {
            break;
        }
        len -= bytes_written;
    }
    return pos;
}