static MagickBooleanType WriteAnimatedWEBPImage(const ImageInfo *image_info,
  Image *image,WebPConfig *configure,WebPMemoryWriter *writer_info,
  ExceptionInfo *exception)
{
  Image
    *first_image;

  PictureMemory
    *current,
    *head;

  size_t
    effective_delta = 0,
    frame_timestamp = 0;

  WebPAnimEncoder
    *enc;

  WebPAnimEncoderOptions
    enc_options;

  WebPData
    webp_data;

  WebPPicture
    picture;

  WebPAnimEncoderOptionsInit(&enc_options);
  if (image_info->verbose)
    enc_options.verbose = 1;

  image=CoalesceImages(image, exception);
  first_image=image;
  enc=WebPAnimEncoderNew((int) image->page.width,(int) image->page.height,
    &enc_options);

  head=(PictureMemory *) calloc(sizeof(*head),1);
  current=head;

  while (image != NULL)
  {
    if (WebPPictureInit(&picture) == 0)
      ThrowWriterException(ResourceLimitError,"UnableToEncodeImageFile");

    WriteSingleWEBPImage(image_info, image, &picture, current, exception);

    effective_delta = image->delay*1000/image->ticks_per_second;
    if (effective_delta < 10)
      effective_delta = 100; /* Consistent with gif2webp */
    frame_timestamp+=effective_delta;

    WebPAnimEncoderAdd(enc,&picture,(int) frame_timestamp,configure);

    image = GetNextImageInList(image);
    current->next=(PictureMemory *) calloc(sizeof(*head), 1);
    current = current->next;
  }
  webp_data.bytes=writer_info->mem;
  webp_data.size=writer_info->size;
  WebPAnimEncoderAssemble(enc, &webp_data);
  WebPMemoryWriterClear(writer_info);
  writer_info->size=webp_data.size;
  writer_info->mem=(unsigned char *) webp_data.bytes;
  WebPAnimEncoderDelete(enc);
  DestroyImageList(first_image);
  FreePictureMemoryList(head);
  return(MagickTrue);
}