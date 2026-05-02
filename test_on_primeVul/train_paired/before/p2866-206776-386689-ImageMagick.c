MagickExport Image *SampleImage(const Image *image,const size_t columns,
  const size_t rows,ExceptionInfo *exception)
{
#define SampleImageTag  "Sample/Image"

  CacheView
    *image_view,
    *sample_view;

  Image
    *sample_image;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  register ssize_t
    x1;

  ssize_t
    *x_offset,
    y;

  PointInfo
    sample_offset;

  /*
    Initialize sampled image attributes.
  */
  assert(image != (const Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  if ((columns == 0) || (rows == 0))
    ThrowImageException(ImageError,"NegativeOrZeroImageSize");
  if ((columns == image->columns) && (rows == image->rows))
    return(CloneImage(image,0,0,MagickTrue,exception));
  sample_image=CloneImage(image,columns,rows,MagickTrue,exception);
  if (sample_image == (Image *) NULL)
    return((Image *) NULL);
  /*
    Set the sampling offset, default is in the mid-point of sample regions.
  */
  sample_offset.x=sample_offset.y=0.5-MagickEpsilon;
  {
    const char
      *value;

    value=GetImageArtifact(image,"sample:offset");
    if (value != (char *) NULL)
      {
        GeometryInfo
          geometry_info;

        MagickStatusType
          flags;

        (void) ParseGeometry(value,&geometry_info);
        flags=ParseGeometry(value,&geometry_info);
        sample_offset.x=sample_offset.y=geometry_info.rho/100.0-MagickEpsilon;
        if ((flags & SigmaValue) != 0)
          sample_offset.y=geometry_info.sigma/100.0-MagickEpsilon;
      }
  }
  /*
    Allocate scan line buffer and column offset buffers.
  */
  x_offset=(ssize_t *) AcquireQuantumMemory((size_t) sample_image->columns,
    sizeof(*x_offset));
  if (x_offset == (ssize_t *) NULL)
    {
      sample_image=DestroyImage(sample_image);
      ThrowImageException(ResourceLimitError,"MemoryAllocationFailed");
    }
  for (x1=0; x1 < (ssize_t) sample_image->columns; x1++)
    x_offset[x1]=(ssize_t) ((((double) x1+sample_offset.x)*image->columns)/
      sample_image->columns);
  /*
    Sample each row.
  */
  status=MagickTrue;
  progress=0;
  image_view=AcquireVirtualCacheView(image,exception);
  sample_view=AcquireAuthenticCacheView(sample_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static,4) shared(status) \
    magick_threads(image,sample_image,1,1)
#endif
  for (y=0; y < (ssize_t) sample_image->rows; y++)
  {
    register const Quantum
      *magick_restrict p;

    register Quantum
      *magick_restrict q;

    register ssize_t
      x;

    ssize_t
      y_offset;

    if (status == MagickFalse)
      continue;
    y_offset=(ssize_t) ((((double) y+sample_offset.y)*image->rows)/
      sample_image->rows);
    p=GetCacheViewVirtualPixels(image_view,0,y_offset,image->columns,1,
      exception);
    q=QueueCacheViewAuthenticPixels(sample_view,0,y,sample_image->columns,1,
      exception);
    if ((p == (const Quantum *) NULL) || (q == (Quantum *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    /*
      Sample each column.
    */
    for (x=0; x < (ssize_t) sample_image->columns; x++)
    {
      register ssize_t
        i;

      if (GetPixelWriteMask(sample_image,q) <= (QuantumRange/2))
        {
          q+=GetPixelChannels(sample_image);
          continue;
        }
      for (i=0; i < (ssize_t) GetPixelChannels(sample_image); i++)
      {
        PixelChannel
          channel;

        PixelTrait
          sample_traits,
          traits;

        channel=GetPixelChannelChannel(image,i);
        traits=GetPixelChannelTraits(image,channel);
        sample_traits=GetPixelChannelTraits(sample_image,channel);
        if ((traits == UndefinedPixelTrait) ||
            (sample_traits == UndefinedPixelTrait))
          continue;
        SetPixelChannel(sample_image,channel,p[x_offset[x]*GetPixelChannels(
          image)+i],q);
      }
      q+=GetPixelChannels(sample_image);
    }
    if (SyncCacheViewAuthenticPixels(sample_view,exception) == MagickFalse)
      status=MagickFalse;
    if (image->progress_monitor != (MagickProgressMonitor) NULL)
      {
        MagickBooleanType
          proceed;

#if defined(MAGICKCORE_OPENMP_SUPPORT)
        #pragma omp critical (MagickCore_SampleImage)
#endif
        proceed=SetImageProgress(image,SampleImageTag,progress++,image->rows);
        if (proceed == MagickFalse)
          status=MagickFalse;
      }
  }
  image_view=DestroyCacheView(image_view);
  sample_view=DestroyCacheView(sample_view);
  x_offset=(ssize_t *) RelinquishMagickMemory(x_offset);
  sample_image->type=image->type;
  if (status == MagickFalse)
    sample_image=DestroyImage(sample_image);
  return(sample_image);
}