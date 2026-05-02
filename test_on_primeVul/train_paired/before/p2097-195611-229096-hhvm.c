int64_t BZ2File::readImpl(char * buf, int64_t length) {
  if (length == 0) {
    return 0;
  }
  assertx(m_bzFile);
  int len = BZ2_bzread(m_bzFile, buf, length);
  /* Sometimes libbz2 will return fewer bytes than requested, and set bzerror
   * to BZ_STREAM_END, but it's not actually EOF, and you can keep reading from
   * the file - so, only set EOF after a failed read. This matches PHP5.
   */
  if (len <= 0) {
    setEof(true);
    if (len < 0) {
      return -1;
    }
  }
  return len;
}