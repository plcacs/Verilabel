void ImageInputType::ensureFallbackContent()
{
    if (m_useFallbackContent)
        return;
    setUseFallbackContent();
    reattachFallbackContent();
}
