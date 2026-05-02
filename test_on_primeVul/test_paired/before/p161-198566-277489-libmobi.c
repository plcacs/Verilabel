MOBI_RET mobi_decode_infl(unsigned char *decoded, int *decoded_size, const unsigned char *rule) {
    int pos = *decoded_size;
    char mod = 'i';
    char dir = '<';
    char olddir;
    unsigned char c;
    while ((c = *rule++)) {
        if (c <= 4) {
            mod = (c <= 2) ? 'i' : 'd'; /* insert, delete */
            olddir = dir;
            dir = (c & 2) ? '<' : '>'; /* left, right */
            if (olddir != dir && olddir) {
                pos = (c & 2) ? *decoded_size : 0;
            }
        }
        else if (c > 10 && c < 20) {
            if (dir == '>') {
                pos = *decoded_size;
            }
            pos -= c - 10;
            dir = 0;
            if (pos < 0 || pos > *decoded_size) {
                debug_print("Position setting failed (%s)\n", decoded);
                return MOBI_DATA_CORRUPT;
            }
        }
        else {
            if (mod == 'i') {
                const unsigned char *s = decoded + pos;
                unsigned char *d = decoded + pos + 1;
                const int l = *decoded_size - pos;
                if (l < 0 || d + l > decoded + INDX_INFLBUF_SIZEMAX) {
                    debug_print("Out of buffer in %s at pos: %i\n", decoded, pos);
                    return MOBI_DATA_CORRUPT;
                }
                memmove(d, s, (size_t) l);
                decoded[pos] = c;
                (*decoded_size)++;
                if (dir == '>') { pos++; }
            } else {
                if (dir == '<') { pos--; }
                const unsigned char *s = decoded + pos + 1;
                unsigned char *d = decoded + pos;
                const int l = *decoded_size - pos;
                if (l < 0 || d + l > decoded + INDX_INFLBUF_SIZEMAX) {
                    debug_print("Out of buffer in %s at pos: %i\n", decoded, pos);
                    return MOBI_DATA_CORRUPT;
                }
                if (decoded[pos] != c) {
                    debug_print("Character mismatch in %s at pos: %i (%c != %c)\n", decoded, pos, decoded[pos], c);
                    return MOBI_DATA_CORRUPT;
                }
                memmove(d, s, (size_t) l);
                (*decoded_size)--;
            }
        }
    }
    return MOBI_SUCCESS;
}