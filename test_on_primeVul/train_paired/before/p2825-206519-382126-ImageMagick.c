MagickExport MagickBooleanType SetImageAlphaChannel(Image *image,
  const AlphaChannelOption alpha_type,ExceptionInfo *exception)
{
  CacheView
    *image_view;

  MagickBooleanType
    status;

  ssize_t
    y;

  assert(image != (Image *) NULL);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"...");
  assert(image->signature == MagickCoreSignature);
  status=MagickTrue;
  switch (alpha_type)
  {
    case ActivateAlphaChannel:
    {
      image->alpha_trait=BlendPixelTrait;
      break;
    }
    case AssociateAlphaChannel:
    {
      /*
        Associate alpha.
      */
      status=SetImageStorageClass(image,DirectClass,exception);
      if (status == MagickFalse)
        break;
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        register Quantum
          *magick_restrict q;

        register ssize_t
          x;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (Quantum *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          double
            gamma;

          register ssize_t
            i;

          gamma=QuantumScale*GetPixelAlpha(image,q);
          for (i=0; i < (ssize_t) GetPixelChannels(image); i++)
          {
            PixelChannel channel = GetPixelChannelChannel(image,i);
            PixelTrait traits = GetPixelChannelTraits(image,channel);
            if (channel == AlphaPixelChannel)
              continue;
            if ((traits & UpdatePixelTrait) == 0)
              continue;
            q[i]=ClampToQuantum(gamma*q[i]);
          }
          q+=GetPixelChannels(image);
        }
        if (SyncCacheViewAuthenticPixels(image_view,exception) == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      image->alpha_trait=CopyPixelTrait;
      return(status);
    }
    case BackgroundAlphaChannel:
    {
      /*
        Set transparent pixels to background color.
      */
      if (image->alpha_trait == UndefinedPixelTrait)
        break;
      status=SetImageStorageClass(image,DirectClass,exception);
      if (status == MagickFalse)
        break;
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        register Quantum
          *magick_restrict q;

        register ssize_t
          x;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (Quantum *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          if (GetPixelAlpha(image,q) == TransparentAlpha)
            {
              SetPixelViaPixelInfo(image,&image->background_color,q);
              SetPixelChannel(image,AlphaPixelChannel,TransparentAlpha,q);
            }
          q+=GetPixelChannels(image);
        }
        if (SyncCacheViewAuthenticPixels(image_view,exception) == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      return(status);
    }
    case CopyAlphaChannel:
    {
      image->alpha_trait=UpdatePixelTrait;
      status=CompositeImage(image,image,IntensityCompositeOp,MagickTrue,0,0,
        exception);
      break;
    }
    case DeactivateAlphaChannel:
    {
      if (image->alpha_trait == UndefinedPixelTrait)
        status=SetImageAlpha(image,OpaqueAlpha,exception);
      image->alpha_trait=CopyPixelTrait;
      break;
    }
    case DisassociateAlphaChannel:
    {
      /*
        Disassociate alpha.
      */
      status=SetImageStorageClass(image,DirectClass,exception);
      if (status == MagickFalse)
        break;
      image->alpha_trait=BlendPixelTrait;
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        register Quantum
          *magick_restrict q;

        register ssize_t
          x;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (Quantum *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          double
            gamma,
            Sa;

          register ssize_t
            i;

          Sa=QuantumScale*GetPixelAlpha(image,q);
          gamma=PerceptibleReciprocal(Sa);
          for (i=0; i < (ssize_t) GetPixelChannels(image); i++)
          {
            PixelChannel channel = GetPixelChannelChannel(image,i);
            PixelTrait traits = GetPixelChannelTraits(image,channel);
            if (channel == AlphaPixelChannel)
              continue;
            if ((traits & UpdatePixelTrait) == 0)
              continue;
            q[i]=ClampToQuantum(gamma*q[i]);
          }
          q+=GetPixelChannels(image);
        }
        if (SyncCacheViewAuthenticPixels(image_view,exception) == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      image->alpha_trait=UndefinedPixelTrait;
      return(status);
    }
    case DiscreteAlphaChannel:
    {
      if (image->alpha_trait == UndefinedPixelTrait)
        status=SetImageAlpha(image,OpaqueAlpha,exception);
      image->alpha_trait=UpdatePixelTrait;
      break;
    }
    case ExtractAlphaChannel:
    {
      status=CompositeImage(image,image,AlphaCompositeOp,MagickTrue,0,0,
        exception);
      image->alpha_trait=UndefinedPixelTrait;
      break;
    }
    case OffAlphaChannel:
    {
      image->alpha_trait=UndefinedPixelTrait;
      break;
    }
    case OnAlphaChannel:
    {
      if (image->alpha_trait == UndefinedPixelTrait)
        status=SetImageAlpha(image,OpaqueAlpha,exception);
      image->alpha_trait=BlendPixelTrait;
      break;
    }
    case OpaqueAlphaChannel:
    {
      status=SetImageAlpha(image,OpaqueAlpha,exception);
      break;
    }
    case RemoveAlphaChannel:
    {
      /*
        Remove transparency.
      */
      if (image->alpha_trait == UndefinedPixelTrait)
        break;
      status=SetImageStorageClass(image,DirectClass,exception);
      if (status == MagickFalse)
        break;
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        register Quantum
          *magick_restrict q;

        register ssize_t
          x;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (Quantum *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          FlattenPixelInfo(image,&image->background_color,
            image->background_color.alpha,q,(double) GetPixelAlpha(image,q),q);
          q+=GetPixelChannels(image);
        }
        if (SyncCacheViewAuthenticPixels(image_view,exception) == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      image->alpha_trait=image->background_color.alpha_trait;
      break;
    }
    case SetAlphaChannel:
    {
      if (image->alpha_trait == UndefinedPixelTrait)
        status=SetImageAlpha(image,OpaqueAlpha,exception);
      break;
    }
    case ShapeAlphaChannel:
    {
      /*
        Remove transparency.
      */
      image->alpha_trait=BlendPixelTrait;
      status=SetImageStorageClass(image,DirectClass,exception);
      if (status == MagickFalse)
        break;
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        PixelInfo
          background;

        register Quantum
          *magick_restrict q;

        register ssize_t
          x;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (Quantum *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        ConformPixelInfo(image,&image->background_color,&background,exception);
        background.alpha_trait=BlendPixelTrait;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          background.alpha=GetPixelIntensity(image,q);
          SetPixelViaPixelInfo(image,&background,q);
          q+=GetPixelChannels(image);
        }
        if (SyncCacheViewAuthenticPixels(image_view,exception) == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      break;
    }
    case TransparentAlphaChannel:
    {
      status=SetImageAlpha(image,TransparentAlpha,exception);
      break;
    }
    case UndefinedAlphaChannel:
      break;
  }
  if (status == MagickFalse)
    return(status);
  (void) SetPixelChannelMask(image,image->channel_mask);
  return(SyncImagePixelCache(image,exception));
}