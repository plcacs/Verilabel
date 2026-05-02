void HierarchicalBitmapRequester::PrepareForEncoding(void)
{
#if ACCUSOFT_CODE
  
  BuildCommon();

  if (m_ppEncodingMCU == NULL) {
    m_ppEncodingMCU = (struct Line **)m_pEnviron->AllocMem(sizeof(struct Line *) * m_ucCount *8);
    memset(m_ppEncodingMCU,0,sizeof(struct Line *) * m_ucCount * 8);
  }
  
  if (m_ppDownsampler == NULL) {
    m_ppDownsampler = (class DownsamplerBase **)m_pEnviron->AllocMem(sizeof(class DownsamplerBase *) * m_ucCount);
    memset(m_ppDownsampler,0,sizeof(class DownsamplerBase *) * m_ucCount);
    
    for(UBYTE i = 0;i < m_ucCount;i++) {
      class Component *comp = m_pFrame->ComponentOf(i);
      UBYTE sx = comp->SubXOf();
      UBYTE sy = comp->SubYOf();

      if (sx > 1 || sy > 1) {
        m_ppDownsampler[i] = DownsamplerBase::CreateDownsampler(m_pEnviron,sx,sy,
                                                                m_ulPixelWidth,m_ulPixelHeight,
                                                                m_pFrame->TablesOf()->
                                                                isDownsamplingInterpolated());
        m_bSubsampling     = true;
      }
    }
  }

  if (m_pLargestScale)
    m_pLargestScale->PrepareForEncoding();
#endif
}