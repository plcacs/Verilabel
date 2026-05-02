MagickExport MagickBooleanType SubstituteString(char **string,
  const char *search,const char *replace)
{
  MagickBooleanType
    status;

  register char
    *p;

  size_t
    extent,
    replace_extent,
    search_extent;

  ssize_t
    offset;

  status=MagickFalse;
  search_extent=0,
  replace_extent=0;
  for (p=strchr(*string,*search); p != (char *) NULL; p=strchr(p+1,*search))
  {
    if (search_extent == 0)
      search_extent=strlen(search);
    if (strncmp(p,search,search_extent) != 0)
      continue;
    /*
      We found a match.
    */
    status=MagickTrue;
    if (replace_extent == 0)
      replace_extent=strlen(replace);
    if (replace_extent > search_extent)
      {
        /*
          Make room for the replacement string.
        */
        offset=(ssize_t) (p-(*string));
        extent=strlen(*string)+replace_extent-search_extent+1;
        *string=(char *) ResizeQuantumMemory(*string,extent+MagickPathExtent,
          sizeof(*p));
        if (*string == (char *) NULL)
          ThrowFatalException(ResourceLimitFatalError,"UnableToAcquireString");
        p=(*string)+offset;
      }
    /*
      Replace string.
    */
    if (search_extent != replace_extent)
      (void) memmove(p+replace_extent,p+search_extent,
        strlen(p+search_extent)+1);
    (void) memcpy(p,replace,replace_extent);
    p+=replace_extent-1;
  }
  return(status);
}