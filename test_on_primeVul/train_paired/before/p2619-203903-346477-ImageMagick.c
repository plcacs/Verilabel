MagickExport MagickBooleanType SetImageExtent(Image *image,const size_t columns,
  const size_t rows,ExceptionInfo *exception)
{
  if ((columns == 0) || (rows == 0))
    ThrowBinaryException(ImageError,"NegativeOrZeroImageSize",image->filename);
  image->columns=columns;
  image->rows=rows;
  if ((image->depth == 0) || (image->depth > (8*sizeof(MagickSizeType))))
    ThrowBinaryException(ImageError,"ImageDepthNotSupported",image->filename);
  return(SyncImagePixelCache(image,exception));
}