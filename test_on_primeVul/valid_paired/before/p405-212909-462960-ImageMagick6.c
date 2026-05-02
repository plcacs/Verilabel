static const char *GetMagickPropertyLetter(const ImageInfo *image_info,
  Image *image,const char letter)
{
  char
    value[MaxTextExtent];

  const char
    *string;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  *value='\0';
  string=(char *) NULL;
  switch (letter)
  {
    case 'b':
    {
      /*
        Image size read in - in bytes.
      */
      (void) FormatMagickSize(image->extent,MagickFalse,value);
      if (image->extent == 0)
        (void) FormatMagickSize(GetBlobSize(image),MagickFalse,value);
      break;
    }
    case 'c':
    {
      /*
        Image comment property - empty string by default.
      */
      string=GetImageProperty(image,"comment");
      if (string == (const char *) NULL)
        string="";
      break;
    }
    case 'd':
    {
      /*
        Directory component of filename.
      */
      GetPathComponent(image->magick_filename,HeadPath,value);
      if (*value == '\0')
        string="";
      break;
    }
    case 'e':
    {
      /*
        Filename extension (suffix) of image file.
      */
      GetPathComponent(image->magick_filename,ExtensionPath,value);
      if (*value == '\0')
        string="";
      break;
    }
    case 'f':
    {
      /*
        Filename without directory component.
      */
      GetPathComponent(image->magick_filename,TailPath,value);
      if (*value == '\0')
        string="";
      break;
    }
    case 'g':
    {
      /*
        Image geometry, canvas and offset  %Wx%H+%X+%Y.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20gx%.20g%+.20g%+.20g",
        (double) image->page.width,(double) image->page.height,
        (double) image->page.x,(double) image->page.y);
      break;
    }
    case 'h':
    {
      /*
        Image height (current).
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        (image->rows != 0 ? image->rows : image->magick_rows));
      break;
    }
    case 'i':
    {
      /*
        Filename last used for image (read or write).
      */
      string=image->filename;
      break;
    }
    case 'k':
    {
      /*
        Number of unique colors.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        GetNumberColors(image,(FILE *) NULL,&image->exception));
      break;
    }
    case 'l':
    {
      /*
        Image label property - empty string by default.
      */
      string=GetImageProperty(image,"label");
      if (string == (const char *) NULL)
        string="";
      break;
    }
    case 'm':
    {
      /*
        Image format (file magick).
      */
      string=image->magick;
      break;
    }
    case 'n':
    {
      /*
        Number of images in the list.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        GetImageListLength(image));
      break;
    }
    case 'o':
    {
      /*
        Output Filename - for delegate use only
      */
      string=image_info->filename;
      break;
    }
    case 'p':
    {
      /*
        Image index in current image list -- As 'n' OBSOLETE.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        GetImageIndexInList(image));
      break;
    }
    case 'q':
    {
      /*
        Quantum depth of image in memory.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        MAGICKCORE_QUANTUM_DEPTH);
      break;
    }
    case 'r':
    {
      ColorspaceType
        colorspace;

      /*
        Image storage class and colorspace.
      */
      colorspace=image->colorspace;
      if ((image->columns != 0) && (image->rows != 0) &&
          (SetImageGray(image,&image->exception) != MagickFalse))
        colorspace=GRAYColorspace;
      (void) FormatLocaleString(value,MaxTextExtent,"%s %s %s",
        CommandOptionToMnemonic(MagickClassOptions,(ssize_t)
        image->storage_class),CommandOptionToMnemonic(MagickColorspaceOptions,
        (ssize_t) colorspace),image->matte != MagickFalse ? "Matte" : "" );
      break;
    }
    case 's':
    {
      /*
        Image scene number.
      */
      if (image_info->number_scenes != 0)
        (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
          image_info->scene);
      else
        (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
          image->scene);
      break;
    }
    case 't':
    {
      /*
        Base filename without directory or extension.
      */
      GetPathComponent(image->magick_filename,BasePath,value);
      break;
    }
    case 'u':
    {
      /*
        Unique filename.
      */
      string=image_info->unique;
      break;
    }
    case 'w':
    {
      /*
        Image width (current).
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        (image->columns != 0 ? image->columns : image->magick_columns));
      break;
    }
    case 'x':
    {
      /*
        Image horizontal resolution.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",
        fabs(image->x_resolution) > MagickEpsilon ? image->x_resolution : 72.0);
      break;
    }
    case 'y':
    {
      /*
        Image vertical resolution.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",
        fabs(image->y_resolution) > MagickEpsilon ? image->y_resolution : 72.0);
      break;
    }
    case 'z':
    {
      /*
        Image depth.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        image->depth);
      break;
    }
    case 'A':
    {
      /*
        Image alpha channel.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%s",
         CommandOptionToMnemonic(MagickBooleanOptions,(ssize_t) image->matte));
      break;
    }
    case 'B':
    {
      /*
        Image size read in - in bytes.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        image->extent);
      if (image->extent == 0)
        (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
          GetBlobSize(image));
      break;
    }
    case 'C':
    {
      /*
        Image compression method.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%s",
        CommandOptionToMnemonic(MagickCompressOptions,(ssize_t)
          image->compression));
      break;
    }
    case 'D':
    {
      /*
        Image dispose method.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%s",
        CommandOptionToMnemonic(MagickDisposeOptions,(ssize_t) image->dispose));
      break;
    }
    case 'F':
    {
      const char
        *q;

      register char
        *p;

      static char
        whitelist[] =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 "
          "$-_.+!*'(),{}|\\^~[]`\"><#%;/?:@&=";

      /*
        Magick filename (sanitized) - filename given incl. coder & read mods.
      */
      (void) CopyMagickString(value,image->magick_filename,MaxTextExtent);
      p=value;
      q=value+strlen(value);
      for (p+=strspn(p,whitelist); p != q; p+=strspn(p,whitelist))
        *p='_';
      break;
    }
    case 'G':
    {
      /*
        Image size as geometry = "%wx%h".
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20gx%.20g",(double)
        image->magick_columns,(double) image->magick_rows);
      break;
    }
    case 'H':
    {
      /*
        Layer canvas height.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        image->page.height);
      break;
    }
    case 'M':
    {
      /*
        Magick filename - filename given incl. coder & read mods.
      */
      string=image->magick_filename;
      break;
    }
    case 'O':
    {
      /*
        Layer canvas offset with sign = "+%X+%Y".
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%+ld%+ld",(long)
        image->page.x,(long) image->page.y);
      break;
    }
    case 'P':
    {
      /*
        Layer canvas page size = "%Wx%H".
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20gx%.20g",(double)
        image->page.width,(double) image->page.height);
      break;
    }
    case 'Q':
    {
      /*
        Image compression quality.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        (image->quality == 0 ? 92 : image->quality));
      break;
    }
    case 'S':
    {
      /*
        Image scenes.
      */
      if (image_info->number_scenes == 0)
        string="2147483647";
      else
        (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
          image_info->scene+image_info->number_scenes);
      break;
    }
    case 'T':
    {
      /*
        Image time delay for animations.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        image->delay);
      break;
    }
    case 'U':
    {
      /*
        Image resolution units.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%s",
        CommandOptionToMnemonic(MagickResolutionOptions,(ssize_t)
          image->units));
      break;
    }
    case 'W':
    {
      /*
        Layer canvas width.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%.20g",(double)
        image->page.width);
      break;
    }
    case 'X':
    {
      /*
        Layer canvas X offset.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%+.20g",(double)
        image->page.x);
      break;
    }
    case 'Y':
    {
      /*
        Layer canvas Y offset.
      */
      (void) FormatLocaleString(value,MaxTextExtent,"%+.20g",(double)
        image->page.y);
      break;
    }
    case 'Z':
    {
      /*
        Zero filename.
      */
      string=image_info->zero;
      break;
    }
    case '@':
    {
      RectangleInfo
        page;

      /*
        Image bounding box.
      */
      page=GetImageBoundingBox(image,&image->exception);
      (void) FormatLocaleString(value,MaxTextExtent,"%.20gx%.20g%+.20g%+.20g",
        (double) page.width,(double) page.height,(double) page.x,(double)
        page.y);
      break;
    }
    case '#':
    {
      /*
        Image signature.
      */
      if ((image->columns != 0) && (image->rows != 0))
        (void) SignatureImage(image);
      string=GetImageProperty(image,"signature");
      break;
    }
    case '%':
    {
      /*
        Percent escaped.
      */
      string="%";
      break;
    }
  }
  if (*value != '\0')
    string=value;
  if (string != (char *) NULL)
    {
      (void) SetImageArtifact(image,"get-property",string);
      return(GetImageArtifact(image,"get-property"));
    }
  return((char *) NULL);
}