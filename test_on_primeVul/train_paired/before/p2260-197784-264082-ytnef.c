BYTE *DecompressRTF(variableLength *p, int *size) {
  BYTE *dst; // destination for uncompressed bytes
  BYTE *src;
  unsigned int in;
  unsigned int out;
  variableLength comp_Prebuf;
  ULONG compressedSize, uncompressedSize, magic;

  comp_Prebuf.size = strlen(RTF_PREBUF);
  comp_Prebuf.data = calloc(comp_Prebuf.size+1, 1);
  ALLOCCHECK_CHAR(comp_Prebuf.data);
  memcpy(comp_Prebuf.data, RTF_PREBUF, comp_Prebuf.size);

  src = p->data;
  in = 0;

  if (p->size < 20) {
    printf("File too small\n");
    return(NULL);
  }
  compressedSize = (ULONG)SwapDWord((BYTE*)src + in, 4);
  in += 4;
  uncompressedSize = (ULONG)SwapDWord((BYTE*)src + in, 4);
  in += 4;
  magic = SwapDWord((BYTE*)src + in, 4);
  in += 4;
  in += 4;

  // check size excluding the size field itself
  if (compressedSize != p->size - 4) {
    printf(" Size Mismatch: %u != %i\n", compressedSize, p->size - 4);
    free(comp_Prebuf.data);
    return NULL;
  }

  // process the data
  if (magic == 0x414c454d) {
    // magic number that identifies the stream as a uncompressed stream
    dst = calloc(uncompressedSize, 1);
    ALLOCCHECK_CHAR(dst);
    memcpy(dst, src + 4, uncompressedSize);
  } else if (magic == 0x75465a4c) {
    // magic number that identifies the stream as a compressed stream
    int flagCount = 0;
    int flags = 0;
    // Prevent overflow on 32 Bit Systems
    if (comp_Prebuf.size >= INT_MAX - uncompressedSize) {
       printf("Corrupted file\n");
       exit(-1);
    }
    dst = calloc(comp_Prebuf.size + uncompressedSize, 1);
    ALLOCCHECK_CHAR(dst);
    memcpy(dst, comp_Prebuf.data, comp_Prebuf.size);
    out = comp_Prebuf.size;
    while (out < (comp_Prebuf.size + uncompressedSize)) {
      // each flag byte flags 8 literals/references, 1 per bit
      flags = (flagCount++ % 8 == 0) ? src[in++] : flags >> 1;
      if ((flags & 1) == 1) { // each flag bit is 1 for reference, 0 for literal
        unsigned int offset = src[in++];
        unsigned int length = src[in++];
        unsigned int end;
        offset = (offset << 4) | (length >> 4); // the offset relative to block start
        length = (length & 0xF) + 2; // the number of bytes to copy
        // the decompression buffer is supposed to wrap around back
        // to the beginning when the end is reached. we save the
        // need for such a buffer by pointing straight into the data
        // buffer, and simulating this behaviour by modifying the
        // pointers appropriately.
        offset = (out / 4096) * 4096 + offset;
        if (offset >= out) // take from previous block
          offset -= 4096;
        // note: can't use System.arraycopy, because the referenced
        // bytes can cross through the current out position.
        end = offset + length;
        while ((offset < end) && (out < (comp_Prebuf.size + uncompressedSize))
             && (offset < (comp_Prebuf.size + uncompressedSize)))
          dst[out++] = dst[offset++];
      } else { // literal
        if ((out >= (comp_Prebuf.size + uncompressedSize)) ||
            (in >= p->size)) {
          printf("Corrupted stream\n");
          exit(-1);
        }
        dst[out++] = src[in++];
      }
    }
    // copy it back without the prebuffered data
    src = dst;
    dst = calloc(uncompressedSize, 1);
    ALLOCCHECK_CHAR(dst);
    memcpy(dst, src + comp_Prebuf.size, uncompressedSize);
    free(src);
    *size = uncompressedSize;
    free(comp_Prebuf.data);
    return dst;
  } else { // unknown magic number
    printf("Unknown compression type (magic number %x)\n", magic);
  }
  free(comp_Prebuf.data);
  return NULL;
}