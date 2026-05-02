void HierarchicalBitmapRequester::PrepareForDecoding(void)
{
#if ACCUSOFT_CODE

  UBYTE i;

  BuildCommon();

  if (m_ppDecodingMCU == NULL) {
    m_ppDecodingMCU = (struct Line **)m_pEnviron->AllocMem(sizeof(struct Line *) * m_ucCount*8);
    memset(m_ppDecodingMCU,0,sizeof(struct Line *) * m_ucCount * 8);
  }

  if (m_ppUpsampler == NULL) {
    m_ppUpsampler = (class UpsamplerBase **)m_pEnviron->AllocMem(sizeof(class UpsamplerBase *) * m_ucCount);
    memset(m_ppUpsampler,0,sizeof(class Upsampler *) * m_ucCount);

    for(i = 0;i < m_ucCount;i++) {
      class Component *comp = m_pFrame->ComponentOf(i);
      UBYTE sx = comp->SubXOf();
      UBYTE sy = comp->SubYOf();

      if (sx > 1 || sy > 1) {
        m_ppUpsampler[i] = UpsamplerBase::CreateUpsampler(m_pEnviron,sx,sy,
                                                          m_ulPixelWidth,m_ulPixelHeight,
                                                          m_pFrame->TablesOf()->isChromaCentered());
        m_bSubsampling   = true;
      }
    }
  }

  if (m_pLargestScale)
    m_pLargestScale->PrepareForDecoding();
#endif
}