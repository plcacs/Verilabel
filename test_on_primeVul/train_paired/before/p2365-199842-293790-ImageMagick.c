static MagickBooleanType WriteDPXImage(const ImageInfo *image_info,Image *image,
  ExceptionInfo *exception)
{
  const char
    *value;

  const StringInfo
    *profile;

  DPXInfo
    dpx;

  GeometryInfo
    geometry_info;

  MagickBooleanType
    status;

  MagickOffsetType
    offset;

  MagickStatusType
    flags;

  QuantumInfo
    *quantum_info;

  QuantumType
    quantum_type;

  register const Quantum
    *p;

  register ssize_t
    i;

  size_t
    channels,
    extent;

  ssize_t
    count,
    horizontal_factor,
    vertical_factor,
    y;

  time_t
    seconds;

  unsigned char
    *pixels;

  /*
    Open output image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  horizontal_factor=4;
  vertical_factor=4;
  if (image_info->sampling_factor != (char *) NULL)
    {
      flags=ParseGeometry(image_info->sampling_factor,&geometry_info);
      horizontal_factor=(ssize_t) geometry_info.rho;
      vertical_factor=(ssize_t) geometry_info.sigma;
      if ((flags & SigmaValue) == 0)
        vertical_factor=horizontal_factor;
      if ((horizontal_factor != 1) && (horizontal_factor != 2) &&
          (horizontal_factor != 4) && (vertical_factor != 1) &&
          (vertical_factor != 2) && (vertical_factor != 4))
        ThrowWriterException(CorruptImageError,"UnexpectedSamplingFactor");
    }
  if ((image->colorspace == YCbCrColorspace) &&
      ((horizontal_factor == 2) || (vertical_factor == 2)))
    if ((image->columns % 2) != 0)
      image->columns++;
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(status);
  /*
    Write file header.
  */
  (void) memset(&dpx,0,sizeof(dpx));
  offset=0;
  dpx.file.magic=0x53445058U;
  offset+=WriteBlobLong(image,dpx.file.magic);
  dpx.file.image_offset=0x2000U;
  profile=GetImageProfile(image,"dpx:user-data");
  if (profile != (StringInfo *) NULL)
    {
      if (GetStringInfoLength(profile) > 1048576)
        ThrowWriterException(ImageError,"WidthOrHeightExceedsLimit");
      dpx.file.image_offset+=(unsigned int) GetStringInfoLength(profile);
      dpx.file.image_offset=(((dpx.file.image_offset+0x2000-1)/0x2000)*0x2000);
    }
  offset+=WriteBlobLong(image,dpx.file.image_offset);
  (void) strncpy(dpx.file.version,"V2.0",sizeof(dpx.file.version)-1);
  offset+=WriteBlob(image,8,(unsigned char *) &dpx.file.version);
  channels=1;
  if (IsImageGray(image) == MagickFalse)
    channels=3;
  if (image->alpha_trait != UndefinedPixelTrait)
    channels++;
  dpx.file.file_size=(unsigned int) (channels*image->columns*image->rows+
    dpx.file.image_offset);
  offset+=WriteBlobLong(image,dpx.file.file_size);
  dpx.file.ditto_key=1U;  /* new frame */
  offset+=WriteBlobLong(image,dpx.file.ditto_key);
  dpx.file.generic_size=0x00000680U;
  offset+=WriteBlobLong(image,dpx.file.generic_size);
  dpx.file.industry_size=0x00000180U;
  offset+=WriteBlobLong(image,dpx.file.industry_size);
  dpx.file.user_size=0;
  if (profile != (StringInfo *) NULL)
    {
      dpx.file.user_size+=(unsigned int) GetStringInfoLength(profile);
      dpx.file.user_size=(((dpx.file.user_size+0x2000-1)/0x2000)*0x2000);
    }
  offset+=WriteBlobLong(image,dpx.file.user_size);
  value=GetDPXProperty(image,"dpx:file.filename",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.file.filename,value,sizeof(dpx.file.filename)-1);
  offset+=WriteBlob(image,sizeof(dpx.file.filename),(unsigned char *)
    dpx.file.filename);
  seconds=time((time_t *) NULL);
  (void) FormatMagickTime(seconds,sizeof(dpx.file.timestamp),
    dpx.file.timestamp);
  offset+=WriteBlob(image,sizeof(dpx.file.timestamp),(unsigned char *)
    dpx.file.timestamp);
  (void) strncpy(dpx.file.creator,MagickAuthoritativeURL,
    sizeof(dpx.file.creator)-1);
  value=GetDPXProperty(image,"dpx:file.creator",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.file.creator,value,sizeof(dpx.file.creator)-1);
  offset+=WriteBlob(image,sizeof(dpx.file.creator),(unsigned char *)
    dpx.file.creator);
  value=GetDPXProperty(image,"dpx:file.project",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.file.project,value,sizeof(dpx.file.project)-1);
  offset+=WriteBlob(image,sizeof(dpx.file.project),(unsigned char *)
    dpx.file.project);
  value=GetDPXProperty(image,"dpx:file.copyright",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.file.copyright,value,sizeof(dpx.file.copyright)-1);
  offset+=WriteBlob(image,sizeof(dpx.file.copyright),(unsigned char *)
    dpx.file.copyright);
  dpx.file.encrypt_key=(~0U);
  offset+=WriteBlobLong(image,dpx.file.encrypt_key);
  offset+=WriteBlob(image,sizeof(dpx.file.reserve),(unsigned char *)
    dpx.file.reserve);
  /*
    Write image header.
  */
  switch (image->orientation)
  {
    default:
    case TopLeftOrientation: dpx.image.orientation=0; break;
    case TopRightOrientation: dpx.image.orientation=1; break;
    case BottomLeftOrientation: dpx.image.orientation=2; break;
    case BottomRightOrientation: dpx.image.orientation=3; break;
    case LeftTopOrientation: dpx.image.orientation=4; break;
    case RightTopOrientation: dpx.image.orientation=5; break;
    case LeftBottomOrientation: dpx.image.orientation=6; break;
    case RightBottomOrientation: dpx.image.orientation=7; break;
  }
  offset+=WriteBlobShort(image,dpx.image.orientation);
  dpx.image.number_elements=1;
  offset+=WriteBlobShort(image,dpx.image.number_elements);
  if ((image->columns != (unsigned int) image->columns) ||
      (image->rows != (unsigned int) image->rows))
    ThrowWriterException(ImageError,"WidthOrHeightExceedsLimit");
  offset+=WriteBlobLong(image,(unsigned int) image->columns);
  offset+=WriteBlobLong(image,(unsigned int) image->rows);
  for (i=0; i < 8; i++)
  {
    dpx.image.image_element[i].data_sign=0U;
    offset+=WriteBlobLong(image,dpx.image.image_element[i].data_sign);
    dpx.image.image_element[i].low_data=0U;
    offset+=WriteBlobLong(image,dpx.image.image_element[i].low_data);
    dpx.image.image_element[i].low_quantity=0.0f;
    offset+=WriteBlobFloat(image,dpx.image.image_element[i].low_quantity);
    dpx.image.image_element[i].high_data=0U;
    offset+=WriteBlobLong(image,dpx.image.image_element[i].high_data);
    dpx.image.image_element[i].high_quantity=0.0f;
    offset+=WriteBlobFloat(image,dpx.image.image_element[i].high_quantity);
    dpx.image.image_element[i].descriptor=0;
    if (i == 0)
      switch (image->colorspace)
      {
        case Rec601YCbCrColorspace:
        case Rec709YCbCrColorspace:
        case YCbCrColorspace:
        {
          dpx.image.image_element[i].descriptor=CbYCr444ComponentType;
          if (image->alpha_trait != UndefinedPixelTrait)
            dpx.image.image_element[i].descriptor=CbYCrA4444ComponentType;
          break;
        }
        default:
        {
          dpx.image.image_element[i].descriptor=RGBComponentType;
          if (image->alpha_trait != UndefinedPixelTrait)
            dpx.image.image_element[i].descriptor=RGBAComponentType;
          if ((image_info->type != TrueColorType) &&
              (image->alpha_trait == UndefinedPixelTrait) &&
              (SetImageGray(image,exception) != MagickFalse))
            dpx.image.image_element[i].descriptor=LumaComponentType;
          break;
        }
      }
    offset+=WriteBlobByte(image,dpx.image.image_element[i].descriptor);
    dpx.image.image_element[i].transfer_characteristic=0;
    if (image->colorspace == LogColorspace)
      dpx.image.image_element[0].transfer_characteristic=
        PrintingDensityColorimetric;
    offset+=WriteBlobByte(image,
      dpx.image.image_element[i].transfer_characteristic);
    dpx.image.image_element[i].colorimetric=0;
    offset+=WriteBlobByte(image,dpx.image.image_element[i].colorimetric);
    dpx.image.image_element[i].bit_size=0;
    if (i == 0)
      dpx.image.image_element[i].bit_size=(unsigned char) image->depth;
    offset+=WriteBlobByte(image,dpx.image.image_element[i].bit_size);
    dpx.image.image_element[i].packing=0;
    if ((image->depth == 10) || (image->depth == 12))
      dpx.image.image_element[i].packing=1;
    offset+=WriteBlobShort(image,dpx.image.image_element[i].packing);
    dpx.image.image_element[i].encoding=0;
    offset+=WriteBlobShort(image,dpx.image.image_element[i].encoding);
    dpx.image.image_element[i].data_offset=0U;
    if (i == 0)
      dpx.image.image_element[i].data_offset=dpx.file.image_offset;
    offset+=WriteBlobLong(image,dpx.image.image_element[i].data_offset);
    dpx.image.image_element[i].end_of_line_padding=0U;
    offset+=WriteBlobLong(image,dpx.image.image_element[i].end_of_line_padding);
    offset+=WriteBlobLong(image,
      dpx.image.image_element[i].end_of_image_padding);
    offset+=WriteBlob(image,sizeof(dpx.image.image_element[i].description),
      (unsigned char *) dpx.image.image_element[i].description);
  }
  offset+=WriteBlob(image,sizeof(dpx.image.reserve),(unsigned char *)
    dpx.image.reserve);
  /*
    Write orientation header.
  */
  if ((image->rows != image->magick_rows) ||
      (image->columns != image->magick_columns))
    {
      /*
        These properties are not valid if image size changed.
      */
      (void) DeleteImageProperty(image,"dpx:orientation.x_offset");
      (void) DeleteImageProperty(image,"dpx:orientation.y_offset");
      (void) DeleteImageProperty(image,"dpx:orientation.x_center");
      (void) DeleteImageProperty(image,"dpx:orientation.y_center");
      (void) DeleteImageProperty(image,"dpx:orientation.x_size");
      (void) DeleteImageProperty(image,"dpx:orientation.y_size");
    }
  dpx.orientation.x_offset=0U;
  value=GetDPXProperty(image,"dpx:orientation.x_offset",exception);
  if (value != (const char *) NULL)
    dpx.orientation.x_offset=(unsigned int) StringToUnsignedLong(value);
  offset+=WriteBlobLong(image,dpx.orientation.x_offset);
  dpx.orientation.y_offset=0U;
  value=GetDPXProperty(image,"dpx:orientation.y_offset",exception);
  if (value != (const char *) NULL)
    dpx.orientation.y_offset=(unsigned int) StringToUnsignedLong(value);
  offset+=WriteBlobLong(image,dpx.orientation.y_offset);
  dpx.orientation.x_center=0.0f;
  value=GetDPXProperty(image,"dpx:orientation.x_center",exception);
  if (value != (const char *) NULL)
    dpx.orientation.x_center=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.orientation.x_center);
  dpx.orientation.y_center=0.0f;
  value=GetDPXProperty(image,"dpx:orientation.y_center",exception);
  if (value != (const char *) NULL)
    dpx.orientation.y_center=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.orientation.y_center);
  dpx.orientation.x_size=0U;
  value=GetDPXProperty(image,"dpx:orientation.x_size",exception);
  if (value != (const char *) NULL)
    dpx.orientation.x_size=(unsigned int) StringToUnsignedLong(value);
  offset+=WriteBlobLong(image,dpx.orientation.x_size);
  dpx.orientation.y_size=0U;
  value=GetDPXProperty(image,"dpx:orientation.y_size",exception);
  if (value != (const char *) NULL)
    dpx.orientation.y_size=(unsigned int) StringToUnsignedLong(value);
  offset+=WriteBlobLong(image,dpx.orientation.y_size);
  value=GetDPXProperty(image,"dpx:orientation.filename",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.orientation.filename,value,
      sizeof(dpx.orientation.filename)-1);
  offset+=WriteBlob(image,sizeof(dpx.orientation.filename),(unsigned char *)
    dpx.orientation.filename);
  offset+=WriteBlob(image,sizeof(dpx.orientation.timestamp),(unsigned char *)
    dpx.orientation.timestamp);
  value=GetDPXProperty(image,"dpx:orientation.device",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.orientation.device,value,
      sizeof(dpx.orientation.device)-1);
  offset+=WriteBlob(image,sizeof(dpx.orientation.device),(unsigned char *)
    dpx.orientation.device);
  value=GetDPXProperty(image,"dpx:orientation.serial",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.orientation.serial,value,
      sizeof(dpx.orientation.serial)-1);
  offset+=WriteBlob(image,sizeof(dpx.orientation.serial),(unsigned char *)
    dpx.orientation.serial);
  for (i=0; i < 4; i++)
    dpx.orientation.border[i]=0;
  value=GetDPXProperty(image,"dpx:orientation.border",exception);
  if (value != (const char *) NULL)
    {
      flags=ParseGeometry(value,&geometry_info);
      if ((flags & SigmaValue) == 0)
        geometry_info.sigma=geometry_info.rho;
      dpx.orientation.border[0]=(unsigned short) (geometry_info.rho+0.5);
      dpx.orientation.border[1]=(unsigned short) (geometry_info.sigma+0.5);
      dpx.orientation.border[2]=(unsigned short) (geometry_info.xi+0.5);
      dpx.orientation.border[3]=(unsigned short) (geometry_info.psi+0.5);
    }
  for (i=0; i < 4; i++)
    offset+=WriteBlobShort(image,dpx.orientation.border[i]);
  for (i=0; i < 2; i++)
    dpx.orientation.aspect_ratio[i]=0U;
  value=GetDPXProperty(image,"dpx:orientation.aspect_ratio",exception);
  if (value != (const char *) NULL)
    {
      flags=ParseGeometry(value,&geometry_info);
      if ((flags & SigmaValue) == 0)
        geometry_info.sigma=geometry_info.rho;
      dpx.orientation.aspect_ratio[0]=(unsigned int) (geometry_info.rho+0.5);
      dpx.orientation.aspect_ratio[1]=(unsigned int) (geometry_info.sigma+0.5);
    }
  for (i=0; i < 2; i++)
    offset+=WriteBlobLong(image,dpx.orientation.aspect_ratio[i]);
  offset+=WriteBlob(image,sizeof(dpx.orientation.reserve),(unsigned char *)
    dpx.orientation.reserve);
  /*
    Write film header.
  */
  (void) memset(dpx.film.id,0,sizeof(dpx.film.id));
  value=GetDPXProperty(image,"dpx:film.id",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.id,value,sizeof(dpx.film.id)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.id),(unsigned char *) dpx.film.id);
  (void) memset(dpx.film.type,0,sizeof(dpx.film.type));
  value=GetDPXProperty(image,"dpx:film.type",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.type,value,sizeof(dpx.film.type)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.type),(unsigned char *)
    dpx.film.type);
  (void) memset(dpx.film.offset,0,sizeof(dpx.film.offset));
  value=GetDPXProperty(image,"dpx:film.offset",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.offset,value,sizeof(dpx.film.offset)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.offset),(unsigned char *)
    dpx.film.offset);
  (void) memset(dpx.film.prefix,0,sizeof(dpx.film.prefix));
  value=GetDPXProperty(image,"dpx:film.prefix",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.prefix,value,sizeof(dpx.film.prefix)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.prefix),(unsigned char *)
    dpx.film.prefix);
  (void) memset(dpx.film.count,0,sizeof(dpx.film.count));
  value=GetDPXProperty(image,"dpx:film.count",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.count,value,sizeof(dpx.film.count)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.count),(unsigned char *)
    dpx.film.count);
  (void) memset(dpx.film.format,0,sizeof(dpx.film.format));
  value=GetDPXProperty(image,"dpx:film.format",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.format,value,sizeof(dpx.film.format)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.format),(unsigned char *)
    dpx.film.format);
  dpx.film.frame_position=0U;
  value=GetDPXProperty(image,"dpx:film.frame_position",exception);
  if (value != (const char *) NULL)
    dpx.film.frame_position=(unsigned int) StringToUnsignedLong(value);
  offset+=WriteBlobLong(image,dpx.film.frame_position);
  dpx.film.sequence_extent=0U;
  value=GetDPXProperty(image,"dpx:film.sequence_extent",exception);
  if (value != (const char *) NULL)
    dpx.film.sequence_extent=(unsigned int) StringToUnsignedLong(value);
  offset+=WriteBlobLong(image,dpx.film.sequence_extent);
  dpx.film.held_count=0U;
  value=GetDPXProperty(image,"dpx:film.held_count",exception);
  if (value != (const char *) NULL)
    dpx.film.held_count=(unsigned int) StringToUnsignedLong(value);
  offset+=WriteBlobLong(image,dpx.film.held_count);
  dpx.film.frame_rate=0.0f;
  value=GetDPXProperty(image,"dpx:film.frame_rate",exception);
  if (value != (const char *) NULL)
    dpx.film.frame_rate=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.film.frame_rate);
  dpx.film.shutter_angle=0.0f;
  value=GetDPXProperty(image,"dpx:film.shutter_angle",exception);
  if (value != (const char *) NULL)
    dpx.film.shutter_angle=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.film.shutter_angle);
  (void) memset(dpx.film.frame_id,0,sizeof(dpx.film.frame_id));
  value=GetDPXProperty(image,"dpx:film.frame_id",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.frame_id,value,sizeof(dpx.film.frame_id)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.frame_id),(unsigned char *)
    dpx.film.frame_id);
  value=GetDPXProperty(image,"dpx:film.slate",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.film.slate,value,sizeof(dpx.film.slate)-1);
  offset+=WriteBlob(image,sizeof(dpx.film.slate),(unsigned char *)
    dpx.film.slate);
  offset+=WriteBlob(image,sizeof(dpx.film.reserve),(unsigned char *)
    dpx.film.reserve);
  /*
    Write television header.
  */
  value=GetDPXProperty(image,"dpx:television.time.code",exception);
  if (value != (const char *) NULL)
    dpx.television.time_code=StringToTimeCode(value);
  offset+=WriteBlobLong(image,dpx.television.time_code);
  value=GetDPXProperty(image,"dpx:television.user.bits",exception);
  if (value != (const char *) NULL)
    dpx.television.user_bits=StringToTimeCode(value);
  offset+=WriteBlobLong(image,dpx.television.user_bits);
  value=GetDPXProperty(image,"dpx:television.interlace",exception);
  if (value != (const char *) NULL)
    dpx.television.interlace=(unsigned char) StringToLong(value);
  offset+=WriteBlobByte(image,dpx.television.interlace);
  value=GetDPXProperty(image,"dpx:television.field_number",exception);
  if (value != (const char *) NULL)
    dpx.television.field_number=(unsigned char) StringToLong(value);
  offset+=WriteBlobByte(image,dpx.television.field_number);
  dpx.television.video_signal=0;
  value=GetDPXProperty(image,"dpx:television.video_signal",exception);
  if (value != (const char *) NULL)
    dpx.television.video_signal=(unsigned char) StringToLong(value);
  offset+=WriteBlobByte(image,dpx.television.video_signal);
  dpx.television.padding=0;
  value=GetDPXProperty(image,"dpx:television.padding",exception);
  if (value != (const char *) NULL)
    dpx.television.padding=(unsigned char) StringToLong(value);
  offset+=WriteBlobByte(image,dpx.television.padding);
  dpx.television.horizontal_sample_rate=0.0f;
  value=GetDPXProperty(image,"dpx:television.horizontal_sample_rate",
    exception);
  if (value != (const char *) NULL)
    dpx.television.horizontal_sample_rate=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.horizontal_sample_rate);
  dpx.television.vertical_sample_rate=0.0f;
  value=GetDPXProperty(image,"dpx:television.vertical_sample_rate",exception);
  if (value != (const char *) NULL)
    dpx.television.vertical_sample_rate=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.vertical_sample_rate);
  dpx.television.frame_rate=0.0f;
  value=GetDPXProperty(image,"dpx:television.frame_rate",exception);
  if (value != (const char *) NULL)
    dpx.television.frame_rate=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.frame_rate);
  dpx.television.time_offset=0.0f;
  value=GetDPXProperty(image,"dpx:television.time_offset",exception);
  if (value != (const char *) NULL)
    dpx.television.time_offset=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.time_offset);
  dpx.television.gamma=0.0f;
  value=GetDPXProperty(image,"dpx:television.gamma",exception);
  if (value != (const char *) NULL)
    dpx.television.gamma=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.gamma);
  dpx.television.black_level=0.0f;
  value=GetDPXProperty(image,"dpx:television.black_level",exception);
  if (value != (const char *) NULL)
    dpx.television.black_level=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.black_level);
  dpx.television.black_gain=0.0f;
  value=GetDPXProperty(image,"dpx:television.black_gain",exception);
  if (value != (const char *) NULL)
    dpx.television.black_gain=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.black_gain);
  dpx.television.break_point=0.0f;
  value=GetDPXProperty(image,"dpx:television.break_point",exception);
  if (value != (const char *) NULL)
    dpx.television.break_point=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.break_point);
  dpx.television.white_level=0.0f;
  value=GetDPXProperty(image,"dpx:television.white_level",exception);
  if (value != (const char *) NULL)
    dpx.television.white_level=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.white_level);
  dpx.television.integration_times=0.0f;
  value=GetDPXProperty(image,"dpx:television.integration_times",exception);
  if (value != (const char *) NULL)
    dpx.television.integration_times=StringToDouble(value,(char **) NULL);
  offset+=WriteBlobFloat(image,dpx.television.integration_times);
  offset+=WriteBlob(image,sizeof(dpx.television.reserve),(unsigned char *)
    dpx.television.reserve);
  /*
    Write user header.
  */
  value=GetDPXProperty(image,"dpx:user.id",exception);
  if (value != (const char *) NULL)
    (void) strncpy(dpx.user.id,value,sizeof(dpx.user.id)-1);
  offset+=WriteBlob(image,sizeof(dpx.user.id),(unsigned char *) dpx.user.id);
  if (profile != (StringInfo *) NULL)
    offset+=WriteBlob(image,GetStringInfoLength(profile),
      GetStringInfoDatum(profile));
  while (offset < (MagickOffsetType) dpx.image.image_element[0].data_offset)
  {
    count=WriteBlobByte(image,0x00);
    if (count != 1)
      {
        ThrowFileException(exception,FileOpenError,"UnableToWriteFile",
          image->filename);
        break;
      }
    offset+=count;
  }
  /*
    Convert pixel packets to DPX raster image.
  */
  quantum_info=AcquireQuantumInfo(image_info,image);
  if (quantum_info == (QuantumInfo *) NULL)
    ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
  SetQuantumQuantum(quantum_info,32);
  SetQuantumPack(quantum_info,dpx.image.image_element[0].packing == 0 ?
    MagickTrue : MagickFalse);
  quantum_type=RGBQuantum;
  if (image->alpha_trait != UndefinedPixelTrait)
    quantum_type=RGBAQuantum;
  if (image->colorspace == YCbCrColorspace)
    {
      quantum_type=CbYCrQuantum;
      if (image->alpha_trait != UndefinedPixelTrait)
        quantum_type=CbYCrAQuantum;
      if ((horizontal_factor == 2) || (vertical_factor == 2))
        quantum_type=CbYCrYQuantum;
    }
  extent=GetBytesPerRow(image->columns,
    image->alpha_trait != UndefinedPixelTrait ? 4UL : 3UL,image->depth,
    dpx.image.image_element[0].packing == 0 ? MagickFalse : MagickTrue);
  if ((image_info->type != TrueColorType) &&
      (image->alpha_trait == UndefinedPixelTrait) &&
      (SetImageGray(image,exception) != MagickFalse))
    {
      quantum_type=GrayQuantum;
      extent=GetBytesPerRow(image->columns,1UL,image->depth,
        dpx.image.image_element[0].packing == 0 ? MagickFalse : MagickTrue);
    }
  pixels=(unsigned char *) GetQuantumPixels(quantum_info);
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    size_t
      length;

    p=GetVirtualPixels(image,0,y,image->columns,1,exception);
    if (p == (const Quantum *) NULL)
      break;
    length=ExportQuantumPixels(image,(CacheView *) NULL,quantum_info,
      quantum_type,pixels,exception);
    count=WriteBlob(image,extent,pixels);
    if (count != (ssize_t) length)
      break;
    status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      break;
  }
  quantum_info=DestroyQuantumInfo(quantum_info);
  if (y < (ssize_t) image->rows)
    ThrowWriterException(CorruptImageError,"UnableToWriteImageData");
  (void) CloseBlob(image);
  return(status);
}