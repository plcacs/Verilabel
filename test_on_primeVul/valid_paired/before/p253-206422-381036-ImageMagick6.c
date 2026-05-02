static Image *ReadHEICImage(const ImageInfo *image_info,
  ExceptionInfo *exception)
{
  const char
    *option;

  const StringInfo
    *profile;

  heif_item_id
    exif_id;

  Image
    *image;

  int
    count,
    stride_y,
    stride_cb,
    stride_cr;

  MagickBooleanType
    status;

  size_t
    length;

  ssize_t
    y;

  struct heif_context
    *heif_context;

  struct heif_decoding_options
    *decode_options;

  struct heif_error
    error;

  struct heif_image
    *heif_image;

  struct heif_image_handle
    *image_handle;

  const uint8_t
    *p_y,
    *p_cb,
    *p_cr;

  void
    *file_data;

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  if (image_info->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",
      image_info->filename);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  image=AcquireImage(image_info);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(DestroyImageList(image));
  if (GetBlobSize(image) > (MagickSizeType) SSIZE_MAX)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  length=(size_t) GetBlobSize(image);
  file_data=AcquireMagickMemory(length);
  if (file_data == (void *) NULL)
    ThrowReaderException(ResourceLimitError,"MemoryAllocationFailed");
  if (ReadBlob(image,length,(unsigned char *) file_data) != (ssize_t) length)
    {
      file_data=RelinquishMagickMemory(file_data);
      ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
    }
  /*
    Decode HEIF file
  */
  heif_context=heif_context_alloc();
  error=heif_context_read_from_memory_without_copy(heif_context,file_data,
    length,NULL);
  if (IsHeifSuccess(&error,image) == MagickFalse)
    {
      heif_context_free(heif_context);
      file_data=RelinquishMagickMemory(file_data);
      return(DestroyImageList(image));
    }
  image_handle=(struct heif_image_handle *) NULL;
  error=heif_context_get_primary_image_handle(heif_context,&image_handle);
  if (IsHeifSuccess(&error,image) == MagickFalse)
    {
      heif_context_free(heif_context);
      file_data=RelinquishMagickMemory(file_data);
      return(DestroyImageList(image));
    }
#if LIBHEIF_NUMERIC_VERSION >= 0x01040000
  length=heif_image_handle_get_raw_color_profile_size(image_handle);
  if (length > 0)
    {
      unsigned char
        *color_buffer;

      /*
        Read color profile.
      */ 
      if ((MagickSizeType) length > GetBlobSize(image))
        {
          heif_image_handle_release(image_handle);
          heif_context_free(heif_context);
          file_data=RelinquishMagickMemory(file_data);
          ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
        }
      color_buffer=(unsigned char *) AcquireMagickMemory(length);
      if (color_buffer != (unsigned char *) NULL)
        {
          error=heif_image_handle_get_raw_color_profile(image_handle,
            color_buffer);
          if (error.code == 0)
            {
              StringInfo
                *profile;

              profile=BlobToStringInfo(color_buffer,length);
              if (profile != (StringInfo*) NULL)
                {
                  (void) SetImageProfile(image,"icc",profile);
                  profile=DestroyStringInfo(profile);
                }
            }
        }
      color_buffer=(unsigned char *) RelinquishMagickMemory(color_buffer);
    }
#endif
  count=heif_image_handle_get_list_of_metadata_block_IDs(image_handle,"Exif",
    &exif_id,1);
  if (count > 0)
    {
      size_t
        exif_size;

      unsigned char
        *exif_buffer;

      /*
        Read Exif profile.
      */
      exif_size=heif_image_handle_get_metadata_size(image_handle,exif_id);
      if ((MagickSizeType) exif_size > GetBlobSize(image))
        {
          heif_image_handle_release(image_handle);
          heif_context_free(heif_context);
          file_data=RelinquishMagickMemory(file_data);
          ThrowReaderException(CorruptImageError,"InsufficientImageDataInFile");
        }
      exif_buffer=(unsigned char*) AcquireMagickMemory(exif_size);
      if (exif_buffer != (unsigned char*) NULL)
        {
          error=heif_image_handle_get_metadata(image_handle,
            exif_id,exif_buffer);
          if (error.code == 0)
            {
              StringInfo
                *profile;

              /*
                The first 4 byte should be skipped since they indicate the
                offset to the start of the TIFF header of the Exif data.
              */
              profile=(StringInfo*) NULL;
              if (exif_size > 8)
                profile=BlobToStringInfo(exif_buffer+4,(size_t) exif_size-4);
              if (profile != (StringInfo*) NULL)
                {
                  (void) SetImageProfile(image,"exif",profile);
                  profile=DestroyStringInfo(profile);
                }
            }
        }
      exif_buffer=(unsigned char *) RelinquishMagickMemory(exif_buffer);
  }
  /*
    Set image size.
  */
  image->depth=8;
  image->columns=(size_t) heif_image_handle_get_width(image_handle);
  image->rows=(size_t) heif_image_handle_get_height(image_handle);
  if (image_info->ping != MagickFalse)
    {
      image->colorspace=YCbCrColorspace;
      heif_image_handle_release(image_handle);
      heif_context_free(heif_context);
      file_data=RelinquishMagickMemory(file_data);
      return(GetFirstImageInList(image));
    }
  status=SetImageExtent(image,image->columns,image->rows);
  if (status == MagickFalse)
    {
      heif_image_handle_release(image_handle);
      heif_context_free(heif_context);
      file_data=RelinquishMagickMemory(file_data);
      return(DestroyImageList(image));
    }
  /*
    Copy HEIF image into ImageMagick data structures
  */
  (void) SetImageColorspace(image,YCbCrColorspace);
  decode_options=(struct heif_decoding_options *) NULL;
  option=GetImageOption(image_info,"heic:preserve-orientation");
  if (IsStringTrue(option) == MagickTrue)
    {
      decode_options=heif_decoding_options_alloc();
      decode_options->ignore_transformations=1;
    }
  else
    (void) SetImageProperty(image,"exif:Orientation","1");
  error=heif_decode_image(image_handle,&heif_image,heif_colorspace_YCbCr,
    heif_chroma_420,NULL);
  if (IsHeifSuccess(&error,image) == MagickFalse)
    {
      heif_image_handle_release(image_handle);
      heif_context_free(heif_context);
      file_data=RelinquishMagickMemory(file_data);
      return(DestroyImageList(image));
    }
  if (decode_options != (struct heif_decoding_options *) NULL)
    {
      /*
        Correct the width and height of the image.
      */
      image->columns=(size_t) heif_image_get_width(heif_image,heif_channel_Y);
      image->rows=(size_t) heif_image_get_height(heif_image,heif_channel_Y);
      status=SetImageExtent(image,image->columns,image->rows);
      heif_decoding_options_free(decode_options);
      if (status == MagickFalse)
        {
          heif_image_release(heif_image);
          heif_image_handle_release(image_handle);
          heif_context_free(heif_context);
          file_data=RelinquishMagickMemory(file_data);
          return(DestroyImageList(image));
        }
    }
  p_y=heif_image_get_plane_readonly(heif_image,heif_channel_Y,&stride_y);
  p_cb=heif_image_get_plane_readonly(heif_image,heif_channel_Cb,&stride_cb);
  p_cr=heif_image_get_plane_readonly(heif_image,heif_channel_Cr,&stride_cr);
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    PixelPacket
      *q;

    register ssize_t
      x;

    q=QueueAuthenticPixels(image,0,y,image->columns,1,exception);
    if (q == (PixelPacket *) NULL)
      break;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      SetPixelRed(q,ScaleCharToQuantum((unsigned char) p_y[y*
        stride_y+x]));
      SetPixelGreen(q,ScaleCharToQuantum((unsigned char) p_cb[(y/2)*
        stride_cb+x/2]));
      SetPixelBlue(q,ScaleCharToQuantum((unsigned char) p_cr[(y/2)*
        stride_cr+x/2]));
      q++;
    }
    if (SyncAuthenticPixels(image,exception) == MagickFalse)
      break;
  }
  heif_image_release(heif_image);
  heif_image_handle_release(image_handle);
  heif_context_free(heif_context);
  file_data=RelinquishMagickMemory(file_data);
  profile=GetImageProfile(image,"icc");
  if (profile != (const StringInfo *) NULL)
    (void) TransformImageColorspace(image,sRGBColorspace);
  return(GetFirstImageInList(image));
}