MagickExport MagickBooleanType SetImageType(Image *image,const ImageType type,
  ExceptionInfo *exception)
{
  const char
    *artifact;

  ImageInfo
    *image_info;

  MagickBooleanType
    status;

  QuantizeInfo
    *quantize_info;

  assert(image != (Image *) NULL);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"...");
  assert(image->signature == MagickCoreSignature);
  status=MagickTrue;
  image_info=AcquireImageInfo();
  image_info->dither=image->dither;
  artifact=GetImageArtifact(image,"dither");
  if (artifact != (const char *) NULL)
    (void) SetImageOption(image_info,"dither",artifact);
  switch (type)
  {
    case BilevelType:
    {
      if (SetImageMonochrome(image,exception) == MagickFalse)
        {
          status=TransformImageColorspace(image,GRAYColorspace,exception);
          (void) NormalizeImage(image,exception);
          quantize_info=AcquireQuantizeInfo(image_info);
          quantize_info->number_colors=2;
          quantize_info->colorspace=GRAYColorspace;
          status=QuantizeImage(quantize_info,image,exception);
          quantize_info=DestroyQuantizeInfo(quantize_info);
        }
      image->colors=2;
      image->alpha_trait=UndefinedPixelTrait;
      break;
    }
    case GrayscaleType:
    {
      if (SetImageGray(image,exception) == MagickFalse)
        status=TransformImageColorspace(image,GRAYColorspace,exception);
      image->alpha_trait=UndefinedPixelTrait;
      break;
    }
    case GrayscaleAlphaType:
    {
      if (SetImageGray(image,exception) == MagickFalse)
        status=TransformImageColorspace(image,GRAYColorspace,exception);
      if (image->alpha_trait == UndefinedPixelTrait)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel,exception);
      break;
    }
    case PaletteType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace,exception);
      if ((image->storage_class == DirectClass) || (image->colors > 256))
        {
          quantize_info=AcquireQuantizeInfo(image_info);
          quantize_info->number_colors=256;
          status=QuantizeImage(quantize_info,image,exception);
          quantize_info=DestroyQuantizeInfo(quantize_info);
        }
      image->alpha_trait=UndefinedPixelTrait;
      break;
    }
    case PaletteBilevelAlphaType:
    {
      ChannelType
        channel_mask;

      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace,exception);
      if (image->alpha_trait == UndefinedPixelTrait)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel,exception);
      channel_mask=SetImageChannelMask(image,AlphaChannel);
      (void) BilevelImage(image,(double) QuantumRange/2.0,exception);
      (void) SetImageChannelMask(image,channel_mask);
      quantize_info=AcquireQuantizeInfo(image_info);
      status=QuantizeImage(quantize_info,image,exception);
      quantize_info=DestroyQuantizeInfo(quantize_info);
      break;
    }
    case PaletteAlphaType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace,exception);
      if (image->alpha_trait == UndefinedPixelTrait)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel,exception);
      quantize_info=AcquireQuantizeInfo(image_info);
      quantize_info->colorspace=TransparentColorspace;
      status=QuantizeImage(quantize_info,image,exception);
      quantize_info=DestroyQuantizeInfo(quantize_info);
      break;
    }
    case TrueColorType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace,exception);
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass,exception);
      image->alpha_trait=UndefinedPixelTrait;
      break;
    }
    case TrueColorAlphaType:
    {
      if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
        status=TransformImageColorspace(image,sRGBColorspace,exception);
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass,exception);
      if (image->alpha_trait == UndefinedPixelTrait)
        (void) SetImageAlphaChannel(image,OpaqueAlphaChannel,exception);
      break;
    }
    case ColorSeparationType:
    {
      if (image->colorspace != CMYKColorspace)
        {
          if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
            status=TransformImageColorspace(image,sRGBColorspace,exception);
          status=TransformImageColorspace(image,CMYKColorspace,exception);
        }
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass,exception);
      image->alpha_trait=UndefinedPixelTrait;
      break;
    }
    case ColorSeparationAlphaType:
    {
      if (image->colorspace != CMYKColorspace)
        {
          if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
            status=TransformImageColorspace(image,sRGBColorspace,exception);
          status=TransformImageColorspace(image,CMYKColorspace,exception);
        }
      if (image->storage_class != DirectClass)
        status=SetImageStorageClass(image,DirectClass,exception);
      if (image->alpha_trait == UndefinedPixelTrait)
        status=SetImageAlphaChannel(image,OpaqueAlphaChannel,exception);
      break;
    }
    case OptimizeType:
    case UndefinedType:
      break;
  }
  image_info=DestroyImageInfo(image_info);
  if (status == MagickFalse)
    return(status);
  image->type=type;
  return(MagickTrue);
}