MagickExport Image *ReadStream(const ImageInfo *image_info,StreamHandler stream,
  ExceptionInfo *exception)
{
  CacheMethods
    cache_methods;

  Image
    *image;

  ImageInfo
    *read_info;

  /*
    Stream image pixels.
  */
  assert(image_info != (ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  if (image_info->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",
      image_info->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  read_info=CloneImageInfo(image_info);
  read_info->cache=AcquirePixelCache(0);
  GetPixelCacheMethods(&cache_methods);
  cache_methods.get_virtual_pixel_handler=GetVirtualPixelStream;
  cache_methods.get_virtual_pixels_handler=GetVirtualPixelsStream;
  cache_methods.get_virtual_metacontent_from_handler=
    GetVirtualMetacontentFromStream;
  cache_methods.get_authentic_pixels_handler=GetAuthenticPixelsStream;
  cache_methods.queue_authentic_pixels_handler=QueueAuthenticPixelsStream;
  cache_methods.sync_authentic_pixels_handler=SyncAuthenticPixelsStream;
  cache_methods.get_authentic_pixels_from_handler=GetAuthenticPixelsFromStream;
  cache_methods.get_authentic_metacontent_from_handler=
    GetAuthenticMetacontentFromStream;
  cache_methods.get_one_virtual_pixel_from_handler=GetOneVirtualPixelFromStream;
  cache_methods.get_one_authentic_pixel_from_handler=
    GetOneAuthenticPixelFromStream;
  cache_methods.destroy_pixel_handler=DestroyPixelStream;
  SetPixelCacheMethods(read_info->cache,&cache_methods);
  read_info->stream=stream;
  image=ReadImage(read_info,exception);
  if (image != (Image *) NULL)
    InitializePixelChannelMap(image);
  read_info=DestroyImageInfo(read_info);
  return(image);
}