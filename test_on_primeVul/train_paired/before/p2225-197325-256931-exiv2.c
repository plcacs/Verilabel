    long WebPImage::getHeaderOffset(byte *data, long data_size,
                                    byte *header, long header_size) {
        long pos = -1;
        for (long i=0; i < data_size - header_size; i++) {
            if (memcmp(header, &data[i], header_size) == 0) {
                pos = i;
                break;
            }
        }
        return pos;
    }