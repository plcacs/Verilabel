static Cache GetImagePixelCache(Image *image,const MagickBooleanType clone,
  ExceptionInfo *exception)
{
  CacheInfo
    *magick_restrict cache_info;

  MagickBooleanType
    destroy,
    status;

  static MagickSizeType
    cache_timelimit = MagickResourceInfinity,
    cpu_throttle = MagickResourceInfinity,
    cycles = 0;

  status=MagickTrue;
  if (cpu_throttle == MagickResourceInfinity)
    cpu_throttle=GetMagickResourceLimit(ThrottleResource);
  if ((cpu_throttle != 0) && ((cycles++ % 32) == 0))
    MagickDelay(cpu_throttle);
  if (cache_epoch == 0)
    {
      /*
        Set the expire time in seconds.
      */
      cache_timelimit=GetMagickResourceLimit(TimeResource);
      cache_epoch=time((time_t *) NULL);
    }
  if ((cache_timelimit != MagickResourceInfinity) &&
      ((MagickSizeType) (time((time_t *) NULL)-cache_epoch) >= cache_timelimit))
    {
#if defined(ECANCELED)
      errno=ECANCELED;
#endif
      ThrowFatalException(ResourceLimitFatalError,"TimeLimitExceeded");
    }
  LockSemaphoreInfo(image->semaphore);
  assert(image->cache != (Cache) NULL);
  cache_info=(CacheInfo *) image->cache;
#if defined(MAGICKCORE_OPENCL_SUPPORT)
  CopyOpenCLBuffer(cache_info);
#endif
  destroy=MagickFalse;
  if ((cache_info->reference_count > 1) || (cache_info->mode == ReadMode))
    {
      LockSemaphoreInfo(cache_info->semaphore);
      if ((cache_info->reference_count > 1) || (cache_info->mode == ReadMode))
        {
          CacheInfo
            *clone_info;

          Image
            clone_image;

          /*
            Clone pixel cache.
          */
          clone_image=(*image);
          clone_image.semaphore=AcquireSemaphoreInfo();
          clone_image.reference_count=1;
          clone_image.cache=ClonePixelCache(cache_info);
          clone_info=(CacheInfo *) clone_image.cache;
          status=OpenPixelCache(&clone_image,IOMode,exception);
          if (status == MagickFalse)
            clone_info=(CacheInfo *) DestroyPixelCache(clone_info);
          else
            {
              if (clone != MagickFalse)
                status=ClonePixelCacheRepository(clone_info,cache_info,
                  exception);
              if (status != MagickFalse)
                {
                  destroy=MagickTrue;
                  image->cache=clone_info;
                }
            }
          RelinquishSemaphoreInfo(&clone_image.semaphore);
        }
      UnlockSemaphoreInfo(cache_info->semaphore);
    }
  if (destroy != MagickFalse)
    cache_info=(CacheInfo *) DestroyPixelCache(cache_info);
  if (status != MagickFalse)
    {
      /*
        Ensure the image matches the pixel cache morphology.
      */
      image->type=UndefinedType;
      if (ValidatePixelCacheMorphology(image) == MagickFalse)
        {
          status=OpenPixelCache(image,IOMode,exception);
          cache_info=(CacheInfo *) image->cache;
          if (cache_info->type == DiskCache)
            (void) ClosePixelCacheOnDisk(cache_info);
        }
    }
  UnlockSemaphoreInfo(image->semaphore);
  if (status == MagickFalse)
    return((Cache) NULL);
  return(image->cache);
}