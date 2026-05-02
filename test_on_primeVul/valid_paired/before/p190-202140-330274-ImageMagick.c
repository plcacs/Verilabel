MagickExport Image *WaveImage(const Image *image,const double amplitude,
  const double wave_length,const PixelInterpolateMethod method,
  ExceptionInfo *exception)
{
#define WaveImageTag  "Wave/Image"

  CacheView
    *canvas_image_view,
    *wave_view;

  float
    *sine_map;

  Image
    *canvas_image,
    *wave_image;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

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
  canvas_image=CloneImage(image,0,0,MagickTrue,exception);
  if (canvas_image == (Image *) NULL)
    return((Image *) NULL);
  if ((canvas_image->alpha_trait == UndefinedPixelTrait) &&
      (canvas_image->background_color.alpha != OpaqueAlpha))
    (void) SetImageAlpha(canvas_image,OpaqueAlpha,exception);
  wave_image=CloneImage(canvas_image,canvas_image->columns,(size_t)
    (canvas_image->rows+2.0*fabs(amplitude)),MagickTrue,exception);
  if (wave_image == (Image *) NULL)
    {
      canvas_image=DestroyImage(canvas_image);
      return((Image *) NULL);
    }
  if (SetImageStorageClass(wave_image,DirectClass,exception) == MagickFalse)
    {
      canvas_image=DestroyImage(canvas_image);
      wave_image=DestroyImage(wave_image);
      return((Image *) NULL);
    }
  /*
    Allocate sine map.
  */
  sine_map=(float *) AcquireQuantumMemory((size_t) wave_image->columns,
    sizeof(*sine_map));
  if (sine_map == (float *) NULL)
    {
      canvas_image=DestroyImage(canvas_image);
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
  canvas_image_view=AcquireVirtualCacheView(canvas_image,exception);
  wave_view=AcquireAuthenticCacheView(wave_image,exception);
  (void) SetCacheViewVirtualPixelMethod(canvas_image_view,
    BackgroundVirtualPixelMethod);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(progress,status) \
    magick_number_threads(canvas_image,wave_image,wave_image->rows,1)
#endif
  for (y=0; y < (ssize_t) wave_image->rows; y++)
  {
    const Quantum
      *magick_restrict p;

    Quantum
      *magick_restrict q;

    ssize_t
      x;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(canvas_image_view,0,y,canvas_image->columns,1,
      exception);
    q=QueueCacheViewAuthenticPixels(wave_view,0,y,wave_image->columns,1,
      exception);
    if ((p == (const Quantum *) NULL) || (q == (Quantum *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    for (x=0; x < (ssize_t) wave_image->columns; x++)
    {
      status=InterpolatePixelChannels(canvas_image,canvas_image_view,
        wave_image,method,(double) x,(double) (y-sine_map[x]),q,exception);
      if (status == MagickFalse)
        break;
      p+=GetPixelChannels(canvas_image);
      q+=GetPixelChannels(wave_image);
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
        proceed=SetImageProgress(canvas_image,WaveImageTag,progress,
          canvas_image->rows);
        if (proceed == MagickFalse)
          status=MagickFalse;
      }
  }
  wave_view=DestroyCacheView(wave_view);
  canvas_image_view=DestroyCacheView(canvas_image_view);
  canvas_image=DestroyImage(canvas_image);
  sine_map=(float *) RelinquishMagickMemory(sine_map);
  if (status == MagickFalse)
    wave_image=DestroyImage(wave_image);
  return(wave_image);
}