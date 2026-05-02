MagickExport MagickBooleanType IsOptionMember(const char *option,
  const char *options)
{
  char
    **option_list,
    *string;

  int
    number_options;

  MagickBooleanType
    member;

  register ssize_t
    i;

  /*
    Is option a member of the options list?
  */
  if (options == (const char *) NULL)
    return(MagickFalse);
  string=ConstantString(options);
  (void) SubstituteString(&string,","," ");
  option_list=StringToArgv(string,&number_options);
  string=DestroyString(string);
  if (option_list == (char **) NULL)
    return(MagickFalse);
  member=MagickFalse;
  for (i=1; i < (ssize_t) number_options; i++)
  {
    if ((*option_list[i] == '!') &&
        (LocaleCompare(option,option_list[i]+1) == 0))
      break;
    if (GlobExpression(option,option_list[i],MagickTrue) != MagickFalse)
      {
        member=MagickTrue;
        break;
      }
    option_list[i]=DestroyString(option_list[i]);
  }
  for ( ; i < (ssize_t) number_options; i++)
    option_list[i]=DestroyString(option_list[i]);
  option_list=(char **) RelinquishMagickMemory(option_list);
  return(member);
}