_isBidi (const uint32_t *label, size_t llen)
{
  while (llen-- > 0) {
    int bc = uc_bidi_category (*label++);

    if (bc == UC_BIDI_R || bc == UC_BIDI_AL || bc == UC_BIDI_AN)
      return 1;
  }

  return 0;
}