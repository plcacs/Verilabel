static MagickBooleanType sRGBTransformImage(Image *image,
  const ColorspaceType colorspace,ExceptionInfo *exception)
{
#define sRGBTransformImageTag  "RGBTransform/Image"

  CacheView
    *image_view;

  MagickBooleanType
    status;

  MagickOffsetType
    progress;

  PrimaryInfo
    primary_info;

  ssize_t
    i;

  ssize_t
    y;

  TransformPacket
    *x_map,
    *y_map,
    *z_map;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  assert(colorspace != sRGBColorspace);
  assert(colorspace != TransparentColorspace);
  assert(colorspace != UndefinedColorspace);
  status=MagickTrue;
  progress=0;
  switch (colorspace)
  {
    case CMYKColorspace:
    {
      PixelInfo
        zero;

      /*
        Convert RGB to CMYK colorspace.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image,exception) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass,exception) == MagickFalse)
            return(MagickFalse);
        }
      if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
        return(MagickFalse);
      GetPixelInfo(image,&zero);
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        PixelInfo
          pixel;

        ssize_t
          x;

        Quantum
          *magick_restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (Quantum *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        pixel=zero;
        for (x=0; x < (ssize_t) image->columns; x++)
        {
          GetPixelInfoPixel(image,q,&pixel);
          ConvertRGBToCMYK(&pixel);
          SetPixelViaPixelInfo(image,&pixel,q);
          q+=GetPixelChannels(image);
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      image->type=image->alpha_trait == UndefinedPixelTrait ?
        ColorSeparationType : ColorSeparationAlphaType;
      if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case LinearGRAYColorspace:
    {
      /*
        Transform image from sRGB to GRAY.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image,exception) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass,exception) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        ssize_t
          x;

        Quantum
          *magick_restrict q;

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
          MagickRealType
            gray;

          gray=0.212656*GetPixelRed(image,q)+0.715158*GetPixelGreen(image,q)+
            0.072186*GetPixelBlue(image,q);
          SetPixelGray(image,ClampToQuantum(DecodePixelGamma(gray)),q);
          q+=GetPixelChannels(image);
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
        return(MagickFalse);
      image->type=GrayscaleType;
      return(status);
    }
    case GRAYColorspace:
    {
      /*
        Transform image from sRGB to GRAY.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image,exception) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass,exception) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        ssize_t
          x;

        Quantum
          *magick_restrict q;

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
          MagickRealType
            gray;

          gray=0.212656*GetPixelRed(image,q)+0.715158*GetPixelGreen(image,q)+
            0.072186*GetPixelBlue(image,q);
          SetPixelGray(image,ClampToQuantum(gray),q);
          q+=GetPixelChannels(image);
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
        return(MagickFalse);
      image->type=GrayscaleType;
      return(status);
    }
    case CMYColorspace:
    case Adobe98Colorspace:
    case DisplayP3Colorspace:
    case HCLColorspace:
    case HCLpColorspace:
    case HSBColorspace:
    case HSIColorspace:
    case HSLColorspace:
    case HSVColorspace:
    case HWBColorspace:
    case JzazbzColorspace:
    case LabColorspace:
    case LCHColorspace:
    case LCHabColorspace:
    case LCHuvColorspace:
    case LMSColorspace:
    case LuvColorspace:
    case ProPhotoColorspace:
    case xyYColorspace:
    case XYZColorspace:
    case YCbCrColorspace:
    case YDbDrColorspace:
    case YIQColorspace:
    case YPbPrColorspace:
    case YUVColorspace:
    {
      const char
        *value;

      double
        white_luminance;

      /*
        Transform image from sRGB to target colorspace.
      */
      white_luminance=10000.0;
      value=GetImageProperty(image,"white-luminance",exception);
      if (value != (const char *) NULL)
        white_luminance=StringToDouble(value,(char **) NULL);
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image,exception) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass,exception) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        ssize_t
          x;

        Quantum
          *magick_restrict q;

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
            blue,
            green,
            red,
            X,
            Y,
            Z;

          red=(double) GetPixelRed(image,q);
          green=(double) GetPixelGreen(image,q);
          blue=(double) GetPixelBlue(image,q);
          switch (colorspace)
          {
            case Adobe98Colorspace:
            {
              ConvertRGBToAdobe98(red,green,blue,&X,&Y,&Z);
              break;
            }
            case CMYColorspace:
            {
              ConvertRGBToCMY(red,green,blue,&X,&Y,&Z);
              break;
            }
            case DisplayP3Colorspace:
            {
              ConvertRGBToDisplayP3(red,green,blue,&X,&Y,&Z);
              break;
            }
            case HCLColorspace:
            {
              ConvertRGBToHCL(red,green,blue,&X,&Y,&Z);
              break;
            }
            case HCLpColorspace:
            {
              ConvertRGBToHCLp(red,green,blue,&X,&Y,&Z);
              break;
            }
            case HSBColorspace:
            {
              ConvertRGBToHSB(red,green,blue,&X,&Y,&Z);
              break;
            }
            case HSIColorspace:
            {
              ConvertRGBToHSI(red,green,blue,&X,&Y,&Z);
              break;
            }
            case HSLColorspace:
            {
              ConvertRGBToHSL(red,green,blue,&X,&Y,&Z);
              break;
            }
            case HSVColorspace:
            {
              ConvertRGBToHSV(red,green,blue,&X,&Y,&Z);
              break;
            }
            case HWBColorspace:
            {
              ConvertRGBToHWB(red,green,blue,&X,&Y,&Z);
              break;
            }
            case JzazbzColorspace:
            {
              ConvertRGBToJzazbz(red,green,blue,white_luminance,&X,&Y,&Z);
              break;
            }
            case LabColorspace:
            {
              ConvertRGBToLab(red,green,blue,&X,&Y,&Z);
              break;
            }
            case LCHColorspace:
            case LCHabColorspace:
            {
              ConvertRGBToLCHab(red,green,blue,&X,&Y,&Z);
              break;
            }
            case LCHuvColorspace:
            {
              ConvertRGBToLCHuv(red,green,blue,&X,&Y,&Z);
              break;
            }
            case LMSColorspace:
            {
              ConvertRGBToLMS(red,green,blue,&X,&Y,&Z);
              break;
            }
            case LuvColorspace:
            {
              ConvertRGBToLuv(red,green,blue,&X,&Y,&Z);
              break;
            }
            case ProPhotoColorspace:
            {
              ConvertRGBToProPhoto(red,green,blue,&X,&Y,&Z);
              break;
            }
            case xyYColorspace:
            {
              ConvertRGBToxyY(red,green,blue,&X,&Y,&Z);
              break;
            }
            case XYZColorspace:
            {
              ConvertRGBToXYZ(red,green,blue,&X,&Y,&Z);
              break;
            }
            case YCbCrColorspace:
            {
              ConvertRGBToYCbCr(red,green,blue,&X,&Y,&Z);
              break;
            }
            case YDbDrColorspace:
            {
              ConvertRGBToYDbDr(red,green,blue,&X,&Y,&Z);
              break;
            }
            case YIQColorspace:
            {
              ConvertRGBToYIQ(red,green,blue,&X,&Y,&Z);
              break;
            }
            case YPbPrColorspace:
            {
              ConvertRGBToYPbPr(red,green,blue,&X,&Y,&Z);
              break;
            }
            case YUVColorspace:
            {
              ConvertRGBToYUV(red,green,blue,&X,&Y,&Z);
              break;
            }
            default:
            {
              X=QuantumScale*red;
              Y=QuantumScale*green;
              Z=QuantumScale*blue;
              break;
            }
          }
          SetPixelRed(image,ClampToQuantum(QuantumRange*X),q);
          SetPixelGreen(image,ClampToQuantum(QuantumRange*Y),q);
          SetPixelBlue(image,ClampToQuantum(QuantumRange*Z),q);
          q+=GetPixelChannels(image);
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case LogColorspace:
    {
#define DisplayGamma  (1.0/1.7)
#define FilmGamma  0.6
#define ReferenceBlack  95.0
#define ReferenceWhite  685.0

      const char
        *value;

      double
        black,
        density,
        film_gamma,
        gamma,
        reference_black,
        reference_white;

      Quantum
        *logmap;

      /*
        Transform RGB to Log colorspace.
      */
      density=DisplayGamma;
      gamma=DisplayGamma;
      value=GetImageProperty(image,"gamma",exception);
      if (value != (const char *) NULL)
        gamma=PerceptibleReciprocal(StringToDouble(value,(char **) NULL));
      film_gamma=FilmGamma;
      value=GetImageProperty(image,"film-gamma",exception);
      if (value != (const char *) NULL)
        film_gamma=StringToDouble(value,(char **) NULL);
      reference_black=ReferenceBlack;
      value=GetImageProperty(image,"reference-black",exception);
      if (value != (const char *) NULL)
        reference_black=StringToDouble(value,(char **) NULL);
      reference_white=ReferenceWhite;
      value=GetImageProperty(image,"reference-white",exception);
      if (value != (const char *) NULL)
        reference_white=StringToDouble(value,(char **) NULL);
      logmap=(Quantum *) AcquireQuantumMemory((size_t) MaxMap+1UL,
        sizeof(*logmap));
      if (logmap == (Quantum *) NULL)
        ThrowBinaryException(ResourceLimitError,"MemoryAllocationFailed",
          image->filename);
      black=pow(10.0,(reference_black-reference_white)*(gamma/density)*0.002*
        PerceptibleReciprocal(film_gamma));
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
        logmap[i]=ScaleMapToQuantum((double) (MaxMap*(reference_white+
          log10(black+(1.0*i/MaxMap)*(1.0-black))/((gamma/density)*0.002*
          PerceptibleReciprocal(film_gamma)))/1024.0));
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        ssize_t
          x;

        Quantum
          *magick_restrict q;

        if (status == MagickFalse)
          continue;
        q=GetCacheViewAuthenticPixels(image_view,0,y,image->columns,1,
          exception);
        if (q == (Quantum *) NULL)
          {
            status=MagickFalse;
            continue;
          }
        for (x=(ssize_t) image->columns; x != 0; x--)
        {
          double
            blue,
            green,
            red;

          red=(double) DecodePixelGamma((MagickRealType)
            GetPixelRed(image,q));
          green=(double) DecodePixelGamma((MagickRealType)
            GetPixelGreen(image,q));
          blue=(double) DecodePixelGamma((MagickRealType)
            GetPixelBlue(image,q));
          SetPixelRed(image,logmap[ScaleQuantumToMap(ClampToQuantum(red))],q);
          SetPixelGreen(image,logmap[ScaleQuantumToMap(ClampToQuantum(green))],
            q);
          SetPixelBlue(image,logmap[ScaleQuantumToMap(ClampToQuantum(blue))],q);
          q+=GetPixelChannels(image);
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      logmap=(Quantum *) RelinquishMagickMemory(logmap);
      if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    case RGBColorspace:
    case scRGBColorspace:
    {
      /*
        Transform image from sRGB to linear RGB.
      */
      if (image->storage_class == PseudoClass)
        {
          if (SyncImage(image,exception) == MagickFalse)
            return(MagickFalse);
          if (SetImageStorageClass(image,DirectClass,exception) == MagickFalse)
            return(MagickFalse);
        }
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        ssize_t
          x;

        Quantum
          *magick_restrict q;

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
            blue,
            green,
            red;

          red=DecodePixelGamma((MagickRealType) GetPixelRed(image,q));
          green=DecodePixelGamma((MagickRealType) GetPixelGreen(image,q));
          blue=DecodePixelGamma((MagickRealType) GetPixelBlue(image,q));
          SetPixelRed(image,ClampToQuantum(red),q);
          SetPixelGreen(image,ClampToQuantum(green),q);
          SetPixelBlue(image,ClampToQuantum(blue),q);
          q+=GetPixelChannels(image);
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
      }
      image_view=DestroyCacheView(image_view);
      if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
        return(MagickFalse);
      return(status);
    }
    default:
      break;
  }
  /*
    Allocate the tables.
  */
  x_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*x_map));
  y_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*y_map));
  z_map=(TransformPacket *) AcquireQuantumMemory((size_t) MaxMap+1UL,
    sizeof(*z_map));
  if ((x_map == (TransformPacket *) NULL) ||
      (y_map == (TransformPacket *) NULL) ||
      (z_map == (TransformPacket *) NULL))
    {
      if (x_map != (TransformPacket *) NULL)
        x_map=(TransformPacket *) RelinquishMagickMemory(x_map);
      if (y_map != (TransformPacket *) NULL)
        y_map=(TransformPacket *) RelinquishMagickMemory(y_map);
      if (z_map != (TransformPacket *) NULL)
        z_map=(TransformPacket *) RelinquishMagickMemory(z_map);
      ThrowBinaryException(ResourceLimitError,"MemoryAllocationFailed",
        image->filename);
    }
  (void) memset(&primary_info,0,sizeof(primary_info));
  switch (colorspace)
  {
    case OHTAColorspace:
    {
      /*
        Initialize OHTA tables:

          I1 = 0.33333*R+0.33334*G+0.33333*B
          I2 = 0.50000*R+0.00000*G-0.50000*B
          I3 =-0.25000*R+0.50000*G-0.25000*B

        I and Q, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) (0.33333*(double) i);
        x_map[i].y=(MagickRealType) (0.50000*(double) i);
        x_map[i].z=(MagickRealType) (-0.25000*(double) i);
        y_map[i].x=(MagickRealType) (0.33334*(double) i);
        y_map[i].y=(MagickRealType) (0.00000*(double) i);
        y_map[i].z=(MagickRealType) (0.50000*(double) i);
        z_map[i].x=(MagickRealType) (0.33333*(double) i);
        z_map[i].y=(MagickRealType) (-0.50000*(double) i);
        z_map[i].z=(MagickRealType) (-0.25000*(double) i);
      }
      break;
    }
    case Rec601YCbCrColorspace:
    {
      /*
        Initialize YCbCr tables (ITU-R BT.601):

          Y =  0.2988390*R+0.5868110*G+0.1143500*B
          Cb= -0.1687367*R-0.3312640*G+0.5000000*B
          Cr=  0.5000000*R-0.4186880*G-0.0813120*B

        Cb and Cr, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) (0.298839*(double) i);
        x_map[i].y=(MagickRealType) (-0.1687367*(double) i);
        x_map[i].z=(MagickRealType) (0.500000*(double) i);
        y_map[i].x=(MagickRealType) (0.586811*(double) i);
        y_map[i].y=(MagickRealType) (-0.331264*(double) i);
        y_map[i].z=(MagickRealType) (-0.418688*(double) i);
        z_map[i].x=(MagickRealType) (0.114350*(double) i);
        z_map[i].y=(MagickRealType) (0.500000*(double) i);
        z_map[i].z=(MagickRealType) (-0.081312*(double) i);
      }
      break;
    }
    case Rec709YCbCrColorspace:
    {
      /*
        Initialize YCbCr tables (ITU-R BT.709):

          Y =  0.212656*R+0.715158*G+0.072186*B
          Cb= -0.114572*R-0.385428*G+0.500000*B
          Cr=  0.500000*R-0.454153*G-0.045847*B

        Cb and Cr, normally -0.5 through 0.5, are normalized to the range 0
        through QuantumRange.
      */
      primary_info.y=(double) (MaxMap+1.0)/2.0;
      primary_info.z=(double) (MaxMap+1.0)/2.0;
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) (0.212656*(double) i);
        x_map[i].y=(MagickRealType) (-0.114572*(double) i);
        x_map[i].z=(MagickRealType) (0.500000*(double) i);
        y_map[i].x=(MagickRealType) (0.715158*(double) i);
        y_map[i].y=(MagickRealType) (-0.385428*(double) i);
        y_map[i].z=(MagickRealType) (-0.454153*(double) i);
        z_map[i].x=(MagickRealType) (0.072186*(double) i);
        z_map[i].y=(MagickRealType) (0.500000*(double) i);
        z_map[i].z=(MagickRealType) (-0.045847*(double) i);
      }
      break;
    }
    case YCCColorspace:
    {
      /*
        Initialize YCC tables:

          Y =  0.298839*R+0.586811*G+0.114350*B
          C1= -0.298839*R-0.586811*G+0.88600*B
          C2=  0.70100*R-0.586811*G-0.114350*B

        YCC is scaled by 1.3584.  C1 zero is 156 and C2 is at 137.
      */
      primary_info.y=(double) ScaleQuantumToMap(ScaleCharToQuantum(156));
      primary_info.z=(double) ScaleQuantumToMap(ScaleCharToQuantum(137));
      for (i=0; i <= (ssize_t) (0.018*MaxMap); i++)
      {
        x_map[i].x=0.005382*i;
        x_map[i].y=(-0.003296)*i;
        x_map[i].z=0.009410*i;
        y_map[i].x=0.010566*i;
        y_map[i].y=(-0.006471)*i;
        y_map[i].z=(-0.007880)*i;
        z_map[i].x=0.002052*i;
        z_map[i].y=0.009768*i;
        z_map[i].z=(-0.001530)*i;
      }
      for ( ; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=0.298839*(1.099*i-0.099);
        x_map[i].y=(-0.298839)*(1.099*i-0.099);
        x_map[i].z=0.70100*(1.099*i-0.099);
        y_map[i].x=0.586811*(1.099*i-0.099);
        y_map[i].y=(-0.586811)*(1.099*i-0.099);
        y_map[i].z=(-0.586811)*(1.099*i-0.099);
        z_map[i].x=0.114350*(1.099*i-0.099);
        z_map[i].y=0.88600*(1.099*i-0.099);
        z_map[i].z=(-0.114350)*(1.099*i-0.099);
      }
      break;
    }
    default:
    {
      /*
        Linear conversion tables.
      */
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static)
#endif
      for (i=0; i <= (ssize_t) MaxMap; i++)
      {
        x_map[i].x=(MagickRealType) (1.0*(double) i);
        x_map[i].y=(MagickRealType) 0.0;
        x_map[i].z=(MagickRealType) 0.0;
        y_map[i].x=(MagickRealType) 0.0;
        y_map[i].y=(MagickRealType) (1.0*(double) i);
        y_map[i].z=(MagickRealType) 0.0;
        z_map[i].x=(MagickRealType) 0.0;
        z_map[i].y=(MagickRealType) 0.0;
        z_map[i].z=(MagickRealType) (1.0*(double) i);
      }
      break;
    }
  }
  /*
    Convert from sRGB.
  */
  switch (image->storage_class)
  {
    case DirectClass:
    default:
    {
      /*
        Convert DirectClass image.
      */
      image_view=AcquireAuthenticCacheView(image,exception);
#if defined(MAGICKCORE_OPENMP_SUPPORT)
      #pragma omp parallel for schedule(static) shared(status) \
        magick_number_threads(image,image,image->rows,1)
#endif
      for (y=0; y < (ssize_t) image->rows; y++)
      {
        MagickBooleanType
          sync;

        PixelInfo
          pixel;

        Quantum
          *magick_restrict q;

        ssize_t
          x;

        unsigned int
          blue,
          green,
          red;

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
          red=ScaleQuantumToMap(ClampToQuantum((MagickRealType)
            GetPixelRed(image,q)));
          green=ScaleQuantumToMap(ClampToQuantum((MagickRealType)
            GetPixelGreen(image,q)));
          blue=ScaleQuantumToMap(ClampToQuantum((MagickRealType)
            GetPixelBlue(image,q)));
          pixel.red=(x_map[red].x+y_map[green].x+z_map[blue].x)+
            primary_info.x;
          pixel.green=(x_map[red].y+y_map[green].y+z_map[blue].y)+
            primary_info.y;
          pixel.blue=(x_map[red].z+y_map[green].z+z_map[blue].z)+
            primary_info.z;
          SetPixelRed(image,ScaleMapToQuantum(pixel.red),q);
          SetPixelGreen(image,ScaleMapToQuantum(pixel.green),q);
          SetPixelBlue(image,ScaleMapToQuantum(pixel.blue),q);
          q+=GetPixelChannels(image);
        }
        sync=SyncCacheViewAuthenticPixels(image_view,exception);
        if (sync == MagickFalse)
          status=MagickFalse;
        if (image->progress_monitor != (MagickProgressMonitor) NULL)
          {
            MagickBooleanType
              proceed;

#if defined(MAGICKCORE_OPENMP_SUPPORT)
            #pragma omp atomic
#endif
            progress++;
            proceed=SetImageProgress(image,sRGBTransformImageTag,progress,
              image->rows);
            if (proceed == MagickFalse)
              status=MagickFalse;
          }
      }
      image_view=DestroyCacheView(image_view);
      break;
    }
    case PseudoClass:
    {
      unsigned int
        blue,
        green,
        red;

      /*
        Convert PseudoClass image.
      */
      for (i=0; i < (ssize_t) image->colors; i++)
      {
        PixelInfo
          pixel;

        red=ScaleQuantumToMap(ClampToQuantum(image->colormap[i].red));
        green=ScaleQuantumToMap(ClampToQuantum(image->colormap[i].green));
        blue=ScaleQuantumToMap(ClampToQuantum(image->colormap[i].blue));
        pixel.red=x_map[red].x+y_map[green].x+z_map[blue].x+primary_info.x;
        pixel.green=x_map[red].y+y_map[green].y+z_map[blue].y+primary_info.y;
        pixel.blue=x_map[red].z+y_map[green].z+z_map[blue].z+primary_info.z;
        image->colormap[i].red=(double) ScaleMapToQuantum(pixel.red);
        image->colormap[i].green=(double) ScaleMapToQuantum(pixel.green);
        image->colormap[i].blue=(double) ScaleMapToQuantum(pixel.blue);
      }
      (void) SyncImage(image,exception);
      break;
    }
  }
  /*
    Relinquish resources.
  */
  z_map=(TransformPacket *) RelinquishMagickMemory(z_map);
  y_map=(TransformPacket *) RelinquishMagickMemory(y_map);
  x_map=(TransformPacket *) RelinquishMagickMemory(x_map);
  if (SetImageColorspace(image,colorspace,exception) == MagickFalse)
    return(MagickFalse);
  return(status);
}