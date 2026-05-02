static RCoreSymCacheElement *parseDragons(RBinFile *bf, RBuffer *buf, int off, int bits, R_OWN char *file_name) {
	D eprintf ("Dragons at 0x%x\n", off);
	ut64 size = r_buf_size (buf);
	if (off >= size) {
		return NULL;
	}
	size -= off;
	if (!size) {
		return NULL;
	}
	ut8 *b = malloc (size);
	if (!b) {
		return NULL;
	}
	int available = r_buf_read_at (buf, off, b, size);
	if (available != size) {
		eprintf ("Warning: r_buf_read_at failed\n");
		return NULL;
	}
#if 0
	// after the list of sections, there's a bunch of unknown
	// data, brobably dwords, and then the same section list again
	// this function aims to parse it.
	0x00000138 |1a2b b2a1 0300 0000 1a2b b2a1 e055 0000| .+.......+...U..
                         n_segments ----.          .--- how many sections ?
	0x00000148 |0100 0000 ca55 0000 0400 0000 1800 0000| .....U..........
	             .---- how many symbols? 0xc7
	0x00000158 |c700 0000 0000 0000 0000 0000 0104 0000| ................
	0x00000168 |250b e803 0000 0100 0000 0000 bd55 0000| %............U..
	0x00000178 |91bb e903 e35a b42c 93a4 340a 8746 9489| .....Z.,..4..F..
	0x00000188 |0cea 4c40 0c00 0000 0900 0000 0000 0000| ..L@............
	0x00000198 |0000 0000 0000 0000 0000 0000 0000 0000| ................
	0x000001a8 |0080 0000 0000 0000 5f5f 5445 5854 0000| ........__TEXT..
	0x000001b8 |0000 0000 0000 0000 0080 0000 0000 0000| ................
	0x000001c8 |0040 0000 0000 0000 5f5f 4441 5441 0000| .@......__DATA..
	0x000001d8 |0000 0000 0000 0000 00c0 0000 0000 0000| ................
	0x000001e8 |0000 0100 0000 0000 5f5f 4c4c 564d 0000| ........__LLVM..
	0x000001f8 |0000 0000 0000 0000 00c0 0100 0000 0000| ................
	0x00000208 |00c0 0000 0000 0000 5f5f 4c49 4e4b 4544| ........__LINKED
	0x00000218 |4954 0000 0000 0000 0000 0000 d069 0000| IT...........i..
#endif
	// eprintf ("Dragon's magic:\n");
	int magicCombo = 0;
	if (!memcmp ("\x1a\x2b\xb2\xa1", b, 4)) { // 0x130  ?
		magicCombo++;
	}
	if (!memcmp ("\x1a\x2b\xb2\xa1", b + 8, 4)) {
		magicCombo++;
	}
	if (magicCombo != 2) {
		// hack for C22F7494
		available = r_buf_read_at (buf, off - 8, b, size);
		if (available != size) {
			eprintf ("Warning: r_buf_read_at failed\n");
			return NULL;
		}
		if (!memcmp ("\x1a\x2b\xb2\xa1", b, 4)) { // 0x130  ?
			off -= 8;
		} else {
			eprintf ("0x%08x  parsing error: invalid magic retry\n", off);
		}
	}
	D eprintf ("0x%08x  magic  OK\n", off);
	D {
		const int e0ss = r_read_le32 (b + 12);
		eprintf ("0x%08x  eoss   0x%x\n", off + 12, e0ss);
	}
	free (b);
	return r_coresym_cache_element_new (bf, buf, off + 16, bits, file_name);
}