MagickExport Image *ImplodeImage(const Image *image,const double amount,
  const PixelInterpolateMethod method,ExceptionInfo *exception)
{
#define ImplodeImageTag  "Implode/Image"

  CacheView
    *canvas_view,
    *implode_view,
    *interpolate_view;

  double
    radius;

  Image
    *canvas_image,
    *implode_image;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  PointInfo
    center,
    scale;

  ssize_t
    y;

  /*
    Initialize implode image attributes.
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
    (void) SetImageAlphaChannel(canvas_image,OpaqueAlphaChannel,exception);
  implode_image=CloneImage(canvas_image,0,0,MagickTrue,exception);
  if (implode_image == (Image *) NULL)
    {
      canvas_image=DestroyImage(canvas_image);
      return((Image *) NULL);
    }
  if (SetImageStorageClass(implode_image,DirectClass,exception) == MagickFalse)
    {
      canvas_image=DestroyImage(canvas_image);
      implode_image=DestroyImage(implode_image);
      return((Image *) NULL);
    }
  /*
    Compute scaling factor.
  */
  scale.x=1.0;
  scale.y=1.0;
  center.x=0.5*canvas_image->columns;
  center.y=0.5*canvas_image->rows;
  radius=center.x;
  if (canvas_image->columns > canvas_image->rows)
    scale.y=(double) canvas_image->columns/(double) canvas_image->rows;
  else
    if (canvas_image->columns < canvas_image->rows)
      {
        scale.x=(double) canvas_image->rows/(double) canvas_image->columns;
        radius=center.y;
      }
  /*
    Implode image.
  */
  status=MagickTrue;
  progress=0;
  canvas_view=AcquireVirtualCacheView(canvas_image,exception);
  interpolate_view=AcquireVirtualCacheView(canvas_image,exception);
  implode_view=AcquireAuthenticCacheView(implode_image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
  #pragma omp parallel for schedule(static) shared(progress,status) \
    magick_number_threads(canvas_image,implode_image,canvas_image->rows,1)
#endif
  for (y=0; y < (ssize_t) canvas_image->rows; y++)
  {
    double
      distance;

    PointInfo
      delta;

    const Quantum
      *magick_restrict p;

    ssize_t
      x;

    Quantum
      *magick_restrict q;

    if (status == MagickFalse)
      continue;
    p=GetCacheViewVirtualPixels(canvas_view,0,y,canvas_image->columns,1,
      exception);
    q=QueueCacheViewAuthenticPixels(implode_view,0,y,implode_image->columns,1,
      exception);
    if ((p == (const Quantum *) NULL) || (q == (Quantum *) NULL))
      {
        status=MagickFalse;
        continue;
      }
    delta.y=scale.y*(double) (y-center.y);
    for (x=0; x < (ssize_t) canvas_image->columns; x++)
    {
      ssize_t
        i;

      /*
        Determine if the pixel is within an ellipse.
      */
      delta.x=scale.x*(double) (x-center.x);
      distance=delta.x*delta.x+delta.y*delta.y;
      if (distance >= (radius*radius))
        for (i=0; i < (ssize_t) GetPixelChannels(canvas_image); i++)
        {
          PixelChannel channel = GetPixelChannelChannel(canvas_image,i);
          PixelTrait traits = GetPixelChannelTraits(canvas_image,channel);
          PixelTrait implode_traits = GetPixelChannelTraits(implode_image,
            channel);
          if ((traits == UndefinedPixelTrait) ||
              (implode_traits == UndefinedPixelTrait))
            continue;
          SetPixelChannel(implode_image,channel,p[i],q);
        }
      else
        {
          double
            factor;

          /*
            Implode the pixel.
          */
          factor=1.0;
          if (distance > 0.0)
            factor=pow(sin(MagickPI*sqrt((double) distance)/radius/2),-amount);
          status=InterpolatePixelChannels(canvas_image,interpolate_view,
            implode_image,method,(double) (factor*delta.x/scale.x+center.x),
            (double) (factor*delta.y/scale.y+center.y),q,exception);
          if (status == MagickFalse)
            break;
        }
      p+=GetPixelChannels(canvas_image);
      q+=GetPixelChannels(implode_image);
    }
    if (SyncCacheViewAuthenticPixels(implode_view,exception) == MagickFalse)
      status=MagickFalse;
    if (canvas_image->progress_monitor != (MagickProgressMonitor) NULL)
      {
        MagickBooleanType
          proceed;

#if defined(MAGICKCORE_OPENMP_SUPPORT)
        #pragma omp atomic
#endif
        progress++;
        proceed=SetImageProgress(canvas_image,ImplodeImageTag,progress,
          canvas_image->rows);
        if (proceed == MagickFalse)
          status=MagickFalse;
      }
  }
  implode_view=DestroyCacheView(implode_view);
  interpolate_view=DestroyCacheView(interpolate_view);
  canvas_view=DestroyCacheView(canvas_view);
  canvas_image=DestroyImage(canvas_image);
  if (status == MagickFalse)
    implode_image=DestroyImage(implode_image);
  return(implode_image);
}