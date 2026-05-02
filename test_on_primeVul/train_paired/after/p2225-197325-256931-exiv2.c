    long WebPImage::getHeaderOffset(byte* data, long data_size, byte* header, long header_size)
    {
        if (data_size < header_size) { return -1; }
        long pos = -1;
        for (long i=0; i < data_size - header_size; i++) {
            if (memcmp(header, &data[i], header_size) == 0) {
                pos = i;
                break;
            }
        }
        return pos;
    }