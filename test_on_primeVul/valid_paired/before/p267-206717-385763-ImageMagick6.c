MagickExport Image *WaveImage(const Image *image,const double amplitude,
  const double wave_length,ExceptionInfo *exception)
{
#define WaveImageTag  "Wave/Image"

  CacheView
    *image_view,
    *wave_view;

  float
    *sine_map;

  Image
    *wave_image;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  MagickPixelPacket
    zero;

  ssize_t
    i;

  ssize_t
    y;

  /*
    Initialize wave image attributes.
  */
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  wave_image=CloneImage(image,image->columns,(size_t) (image->rows+2.0*
    fabs(amplitude)),MagickTrue,exception);
  if (wave_image == (Image *) NULL)
    return((Image *) NULL);
  if (SetImageStorageClass(wave_image,DirectClass) == MagickFalse)
    {
      InheritException(exception,&wave_image->exception);
      wave_image=DestroyImage(wave_image);
      return((Image *) NULL);
    }
  if (wave_image->background_color.opacity != OpaqueOpacity)
    wave_image->matte=MagickTrue;
  /*
    Allocate sine map.
  */
  sine_map=(float *) AcquireQuantumMemory((size_t) wave_image->columns,
    sizeof(*sine_map));
  if (sine_map == (float *) NULL)
    {
      wave_image=DestroyImage(wave_image);
      ThrowImageException(ResourceLimitError,"MemoryAllocationFailed");
    }
  for (i=0; i < (ssize_t) wave_image->columns; i++)
    sine_map[i]=(float) fabs(amplitude)+amplitude*sin((double)
      ((2.0*MagickPI*i)/wave_length));
  /*
    Wave image.
  */
  status=MagickTrue;
  progress=0;
  GetMagickPixelPacket(wave_image,&zero);
  image_view=AcquireVirtualCacheView(image,exception);
  wave_view=AcquireAuthenticCacheView(wave_image,exception);
  (void) SetCacheViewVirtualPixelMethod(image_view,
    BackgroundVirtualPixelMethod);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(progress,status) \
    magick_number_threads(image,wave_image,wave_image->rows,1)
#endif
  for (y=0; y < (ssize_t) wave_image->rows; y++)
  {
    MagickPixelPacket
      pixel;

    IndexPacket
      *magick_restrict indexes;

    PixelPacket
      *magick_restrict q;

    ssize_t
      x;

    if (status == MagickFalse)
      continue;
    q=QueueCacheViewAuthenticPixels(wave_view,0,y,wave_image->columns,1,
      exception);
    if (q == (PixelPacket *) NULL)
      {
        status=MagickFalse;
        continue;
      }
    indexes=GetCacheViewAuthenticIndexQueue(wave_view);
    pixel=zero;
    for (x=0; x < (ssize_t) wave_image->columns; x++)
    {
      status=InterpolateMagickPixelPacket(image,image_view,
        UndefinedInterpolatePixel,(double) x,(double) (y-sine_map[x]),&pixel,
        exception);
      if (status == MagickFalse)
        break;
      SetPixelPacket(wave_image,&pixel,q,indexes+x);
      q++;
    }
    if (SyncCacheViewAuthenticPixels(wave_view,exception) == MagickFalse)
      status=MagickFalse;
    if (image->progress_monitor != (MagickProgressMonitor) NULL)
      {
        MagickBooleanType
          proceed;

#if defined(MAGICKCORE_OPENMP_SUPPORT)
        #pragma omp atomic
#endif
        progress++;
        proceed=SetImageProgress(image,WaveImageTag,progress,image->rows);
        if (proceed == MagickFalse)
          status=MagickFalse;
      }
  }
  wave_view=DestroyCacheView(wave_view);
  image_view=DestroyCacheView(image_view);
  sine_map=(float *) RelinquishMagickMemory(sine_map);
  if (status == MagickFalse)
    wave_image=DestroyImage(wave_image);
  return(wave_image);
}