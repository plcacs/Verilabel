static MagickBooleanType WriteMSLImage(const ImageInfo *image_info,Image *image)
{
  Image
    *msl_image;

  MagickBooleanType
    status;

  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  msl_image=CloneImage(image,0,0,MagickTrue,&image->exception);
  status=ProcessMSLScript(image_info,&msl_image,&image->exception);
  msl_image=DestroyImageList(msl_image);
  return(status);
}