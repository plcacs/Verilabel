MagickExport MagickBooleanType SetImageProfile(Image *image,const char *name,
  const StringInfo *profile,ExceptionInfo *exception)
{
  char
    key[MaxTextExtent],
    property[MaxTextExtent];

  MagickBooleanType
    status;

  assert(image != (Image *) NULL);
  assert(image->signature == MagickSignature);
  if (image->debug != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  if (image->profiles == (SplayTreeInfo *) NULL)
    image->profiles=NewSplayTree(CompareSplayTreeString,RelinquishMagickMemory,
      DestroyProfile);
  (void) CopyMagickString(key,name,MaxTextExtent);
  status=AddValueToSplayTree((SplayTreeInfo *) image->profiles,
    ConstantString(key),CloneStringInfo(profile));
  if ((status != MagickFalse) &&
      ((LocaleCompare(name,"iptc") == 0) || (LocaleCompare(name,"8bim") == 0)))
    (void) GetProfilesFromResourceBlock(image,profile,exception);
  /*
    Inject profile into image properties.
  */
  (void) FormatLocaleString(property,MaxTextExtent,"%s:sans",name);
  (void) GetImageProperty(image,property,exception);
  return(status);
}