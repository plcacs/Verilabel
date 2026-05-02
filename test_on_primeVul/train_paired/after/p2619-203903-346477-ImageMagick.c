MagickExport MagickBooleanType SetImageExtent(Image *image,const size_t columns,
  const size_t rows,ExceptionInfo *exception)
{
  if ((columns == 0) || (rows == 0))
    ThrowBinaryException(ImageError,"NegativeOrZeroImageSize",image->filename);
  image->columns=columns;
  image->rows=rows;
  if (image->depth == 0)
    {
      image->depth=8;
      (void) ThrowMagickException(exception,GetMagickModule(),ImageError,
        "ImageDepthNotSupported","`%s'",image->filename);
    }
  if (image->depth > (8*sizeof(MagickSizeType)))
    {
      image->depth=8*sizeof(MagickSizeType);
      (void) ThrowMagickException(exception,GetMagickModule(),ImageError,
        "ImageDepthNotSupported","`%s'",image->filename);
    }
  return(SyncImagePixelCache(image,exception));
}