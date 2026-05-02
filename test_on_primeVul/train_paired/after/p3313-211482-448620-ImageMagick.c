MagickExport const char *GetMagickProperty(ImageInfo *image_info,
  Image *image,const char *property,ExceptionInfo *exception)
{
  char
    value[MagickPathExtent];

  const char
    *string;

  assert(property[0] != '\0');
  assert(image != (Image *) NULL || image_info != (ImageInfo *) NULL );
  if (property[1] == '\0')  /* single letter property request */
    return(GetMagickPropertyLetter(image_info,image,*property,exception));
  if ((image != (Image *) NULL) && (image->debug != MagickFalse))
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  else
    if ((image_info != (ImageInfo *) NULL) &&
        (image_info->debug != MagickFalse))
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s","no-images");
  *value='\0';           /* formated string */
  string=(char *) NULL;  /* constant string reference */
  switch (*property)
  {
    case 'b':
    {
      if (LocaleCompare("basename",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          GetPathComponent(image->magick_filename,BasePath,value);
          if (*value == '\0')
            string="";
          break;
        }
      if (LocaleCompare("bit-depth",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
            GetImageDepth(image,exception));
          break;
        }
      break;
    }
    case 'c':
    {
      if (LocaleCompare("channels",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          /* FUTURE: return actual image channels */
          (void) FormatLocaleString(value,MagickPathExtent,"%s",
            CommandOptionToMnemonic(MagickColorspaceOptions,(ssize_t)
            image->colorspace));
          LocaleLower(value);
          if( image->alpha_trait != UndefinedPixelTrait )
            (void) ConcatenateMagickString(value,"a",MagickPathExtent);
          break;
        }
      if (LocaleCompare("colorspace",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickColorspaceOptions,(ssize_t)
            image->colorspace);
          break;
        }
      if (LocaleCompare("compose",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickComposeOptions,(ssize_t)
            image->compose);
          break;
        }
      if (LocaleCompare("compression",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickCompressOptions,(ssize_t)
            image->compression);
          break;
        }
      if (LocaleCompare("copyright",property) == 0)
        {
          (void) CopyMagickString(value,GetMagickCopyright(),MagickPathExtent);
          break;
        }
      break;
    }
    case 'd':
    {
      if (LocaleCompare("depth",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
            image->depth);
          break;
        }
      if (LocaleCompare("directory",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          GetPathComponent(image->magick_filename,HeadPath,value);
          if (*value == '\0')
            string="";
          break;
        }
      break;
    }
    case 'e':
    {
      if (LocaleCompare("entropy",property) == 0)
        {
          double
            entropy;

          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) GetImageEntropy(image,&entropy,exception);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),entropy);
          break;
        }
      if (LocaleCompare("extension",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          GetPathComponent(image->magick_filename,ExtensionPath,value);
          if (*value == '\0')
            string="";
          break;
        }
      break;
    }
    case 'g':
    {
      if (LocaleCompare("gamma",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),image->gamma);
          break;
        }
      break;
    }
    case 'h':
    {
      if (LocaleCompare("height",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.20g",
            image->magick_rows != 0 ? (double) image->magick_rows : 256.0);
          break;
        }
      break;
    }
    case 'i':
    {
      if (LocaleCompare("input",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=image->filename;
          break;
        }
      if (LocaleCompare("interlace",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickInterlaceOptions,(ssize_t)
            image->interlace);
          break;
        }
      break;
    }
    case 'k':
    {
      if (LocaleCompare("kurtosis",property) == 0)
        {
          double
            kurtosis,
            skewness;

          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) GetImageKurtosis(image,&kurtosis,&skewness,exception);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),kurtosis);
          break;
        }
      break;
    }
    case 'm':
    {
      if (LocaleCompare("magick",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=image->magick;
          break;
        }
      if ((LocaleCompare("maxima",property) == 0) ||
          (LocaleCompare("max",property) == 0))
        {
          double
            maximum,
            minimum;

          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) GetImageRange(image,&minimum,&maximum,exception);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),maximum);
          break;
        }
      if (LocaleCompare("mean",property) == 0)
        {
          double
            mean,
            standard_deviation;

          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) GetImageMean(image,&mean,&standard_deviation,exception);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),mean);
          break;
        }
      if ((LocaleCompare("minima",property) == 0) ||
          (LocaleCompare("min",property) == 0))
        {
          double
            maximum,
            minimum;

          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) GetImageRange(image,&minimum,&maximum,exception);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),minimum);
          break;
        }
      break;
    }
    case 'o':
    {
      if (LocaleCompare("opaque",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickBooleanOptions,(ssize_t)
            IsImageOpaque(image,exception));
          break;
        }
      if (LocaleCompare("orientation",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickOrientationOptions,(ssize_t)
            image->orientation);
          break;
        }
      if (LocaleCompare("output",property) == 0)
        {
          WarnNoImageInfoReturn("\"%%[%s]\"",property);
          (void) CopyMagickString(value,image_info->filename,MagickPathExtent);
          break;
        }
      break;
    }
    case 'p':
    {
      if (LocaleCompare("page",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.20gx%.20g",
            (double) image->page.width,(double) image->page.height);
          break;
        }
#if defined(MAGICKCORE_LCMS_DELEGATE)
      if (LocaleCompare("profile:icc",property) == 0 ||
          LocaleCompare("profile:icm",property) == 0)
        {
#if !defined(LCMS_VERSION) || (LCMS_VERSION < 2000)
#define cmsUInt32Number  DWORD
#endif

          const StringInfo
            *profile;

          cmsHPROFILE
            icc_profile;

          WarnNoImageReturn("\"%%[%s]\"",property);
          profile=GetImageProfile(image,property+8);
          if (profile == (StringInfo *) NULL)
            break;
          icc_profile=cmsOpenProfileFromMem(GetStringInfoDatum(profile),
            (cmsUInt32Number) GetStringInfoLength(profile));
          if (icc_profile != (cmsHPROFILE *) NULL)
            {
#if defined(LCMS_VERSION) && (LCMS_VERSION < 2000)
              string=cmsTakeProductName(icc_profile);
#else
              (void) cmsGetProfileInfoASCII(icc_profile,cmsInfoDescription,
                "en","US",value,MagickPathExtent);
#endif
              (void) cmsCloseProfile(icc_profile);
            }
      }
#endif
      if (LocaleCompare("profiles",property) == 0)
        {
          const char
            *name;

          WarnNoImageReturn("\"%%[%s]\"",property);
          ResetImageProfileIterator(image);
          name=GetNextImageProfile(image);
          if (name != (char *) NULL)
            {
              (void) CopyMagickString(value,name,MagickPathExtent);
              name=GetNextImageProfile(image);
              while (name != (char *) NULL)
              {
                ConcatenateMagickString(value,",",MagickPathExtent);
                ConcatenateMagickString(value,name,MagickPathExtent);
                name=GetNextImageProfile(image);
              }
            }
          break;
        }
      break;
    }
    case 'q':
    {
      if (LocaleCompare("quality",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
            image->quality);
          break;
        }
      break;
    }
    case 'r':
    {
      if (LocaleCompare("resolution.x",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%g",
            image->resolution.x);
          break;
        }
      if (LocaleCompare("resolution.y",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%g",
            image->resolution.y);
          break;
        }
      break;
    }
    case 's':
    {
      if (LocaleCompare("scene",property) == 0)
        {
          WarnNoImageInfoReturn("\"%%[%s]\"",property);
          if (image_info->number_scenes != 0)
            (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
              image_info->scene);
          else {
            WarnNoImageReturn("\"%%[%s]\"",property);
            (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
              image->scene);
          }
          break;
        }
      if (LocaleCompare("scenes",property) == 0)
        {
          /* FUTURE: equivelent to %n? */
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
            GetImageListLength(image));
          break;
        }
      if (LocaleCompare("size",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatMagickSize(GetBlobSize(image),MagickFalse,"B",
            MagickPathExtent,value);
          break;
        }
      if (LocaleCompare("skewness",property) == 0)
        {
          double
            kurtosis,
            skewness;

          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) GetImageKurtosis(image,&kurtosis,&skewness,exception);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),skewness);
          break;
        }
      if (LocaleCompare("standard-deviation",property) == 0)
        {
          double
            mean,
            standard_deviation;

          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) GetImageMean(image,&mean,&standard_deviation,exception);
          (void) FormatLocaleString(value,MagickPathExtent,"%.*g",
            GetMagickPrecision(),standard_deviation);
          break;
        }
      break;
    }
    case 't':
    {
      if (LocaleCompare("type",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickTypeOptions,(ssize_t)
            IdentifyImageType(image,exception));
          break;
        }
       break;
    }
    case 'u':
    {
      if (LocaleCompare("unique",property) == 0)
        {
          WarnNoImageInfoReturn("\"%%[%s]\"",property);
          string=image_info->unique;
          break;
        }
      if (LocaleCompare("units",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          string=CommandOptionToMnemonic(MagickResolutionOptions,(ssize_t)
            image->units);
          break;
        }
      break;
    }
    case 'v':
    {
      if (LocaleCompare("version",property) == 0)
        {
          string=GetMagickVersion((size_t *) NULL);
          break;
        }
      break;
    }
    case 'w':
    {
      if (LocaleCompare("width",property) == 0)
        {
          WarnNoImageReturn("\"%%[%s]\"",property);
          (void) FormatLocaleString(value,MagickPathExtent,"%.20g",(double)
            (image->magick_columns != 0 ? image->magick_columns : 256));
          break;
        }
      break;
    }
  }
  if (string != (char *) NULL)
    return(string);
  if (*value != '\0')
    {
      /*
        Create a cloned copy of result, that will get cleaned up, eventually.
      */
      if (image != (Image *) NULL)
        {
          (void) SetImageArtifact(image,"get-property",value);
          return(GetImageArtifact(image,"get-property"));
        }
      else
        {
          (void) SetImageOption(image_info,"get-property",value);
          return(GetImageOption(image_info,"get-property"));
        }
    }
  return((char *) NULL);
}