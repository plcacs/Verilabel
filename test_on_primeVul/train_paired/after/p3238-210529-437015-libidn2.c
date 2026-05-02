_isBidi (const uint32_t *label, size_t llen)
{
  for (; (ssize_t) llen > 0; llen--) {
    int bc = uc_bidi_category (*label++);

    if (bc == UC_BIDI_R || bc == UC_BIDI_AL || bc == UC_BIDI_AN)
      return 1;
  }

  return 0;
}