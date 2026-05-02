WandPrivate void CLINoImageOperator(MagickCLI *cli_wand,
  const char *option,const char *arg1n,const char *arg2n)
{
  const char    /* percent escaped versions of the args */
    *arg1,
    *arg2;

#define _image_info     (cli_wand->wand.image_info)
#define _images         (cli_wand->wand.images)
#define _exception      (cli_wand->wand.exception)
#define _process_flags  (cli_wand->process_flags)
#define _option_type    ((CommandOptionFlags) cli_wand->command->flags)
#define IfNormalOp      (*option=='-')
#define IfPlusOp        (*option!='-')

  assert(cli_wand != (MagickCLI *) NULL);
  assert(cli_wand->signature == MagickWandSignature);
  assert(cli_wand->wand.signature == MagickWandSignature);

  if (cli_wand->wand.debug != MagickFalse)
    (void) CLILogEvent(cli_wand,CommandEvent,GetMagickModule(),
      "- NoImage Operator: %s \"%s\" \"%s\"", option,
      arg1n != (char *) NULL ? arg1n : "",
      arg2n != (char *) NULL ? arg2n : "");

  arg1 = arg1n;
  arg2 = arg2n;

  /* Interpret Percent Escapes in Arguments - using first image */
  if ( (((_process_flags & ProcessInterpretProperities) != 0 )
        || ((_option_type & AlwaysInterpretArgsFlag) != 0)
       )  && ((_option_type & NeverInterpretArgsFlag) == 0) ) {
    /* Interpret Percent escapes in argument 1 */
    if (arg1n != (char *) NULL) {
      arg1=InterpretImageProperties(_image_info,_images,arg1n,_exception);
      if (arg1 == (char *) NULL) {
        CLIWandException(OptionWarning,"InterpretPropertyFailure",option);
        arg1=arg1n;  /* use the given argument as is */
      }
    }
    if (arg2n != (char *) NULL) {
      arg2=InterpretImageProperties(_image_info,_images,arg2n,_exception);
      if (arg2 == (char *) NULL) {
        CLIWandException(OptionWarning,"InterpretPropertyFailure",option);
        arg2=arg2n;  /* use the given argument as is */
      }
    }
  }
#undef _process_flags
#undef _option_type

  do {  /* break to exit code */
    /*
      No-op options  (ignore these)
    */
    if (LocaleCompare("noop",option+1) == 0)   /* zero argument */
      break;
    if (LocaleCompare("sans",option+1) == 0)   /* one argument */
      break;
    if (LocaleCompare("sans0",option+1) == 0)  /* zero argument */
      break;
    if (LocaleCompare("sans1",option+1) == 0)  /* one argument */
      break;
    if (LocaleCompare("sans2",option+1) == 0)  /* two arguments */
      break;
    /*
      Image Reading
    */
    if ( ( LocaleCompare("read",option+1) == 0 ) ||
      ( LocaleCompare("--",option) == 0 ) ) {
      /* Do Glob filename Expansion for 'arg1' then read all images.
      *
      * Expansion handles '@', '~', '*', and '?' meta-characters while ignoring
      * (but attaching to the filenames in the generated argument list) any
      * [...] read modifiers that may be present.
      *
      * For example: It will expand '*.gif[20x20]' into a list such as
      * 'abc.gif[20x20]',  'foobar.gif[20x20]',  'xyzzy.gif[20x20]'
      *
      * NOTE: In IMv6 this was done globally across all images. This
      * meant you could include IM options in '@filename' lists, but you
      * could not include comments.   Doing it only for image read makes
      * it far more secure.
      *
      * Note: arguments do not have percent escapes expanded for security
      * reasons.
      */
      int      argc;
      char     **argv;
      ssize_t  i;

      argc = 1;
      argv = (char **) &arg1;

      /* Expand 'glob' expressions in the given filename.
        Expansion handles any 'coder:' prefix, or read modifiers attached
        to the filename, including them in the resulting expanded list.
      */
      if (ExpandFilenames(&argc,&argv) == MagickFalse)
        CLIWandExceptArgBreak(ResourceLimitError,"MemoryAllocationFailed",
            option,GetExceptionMessage(errno));

      /* loop over expanded filename list, and read then all in */
      for (i=0; i < (ssize_t) argc; i++) {
        Image *
          new_images;
        if (_image_info->ping != MagickFalse)
          new_images=PingImages(_image_info,argv[i],_exception);
        else
          new_images=ReadImages(_image_info,argv[i],_exception);
        AppendImageToList(&_images, new_images);
        argv[i]=DestroyString(argv[i]);
      }
      argv=(char **) RelinquishMagickMemory(argv);
      break;
    }
    /*
      Image Writing
      Note: Writing a empty image list is valid in specific cases
    */
    if (LocaleCompare("write",option+1) == 0) {
      /* Note: arguments do not have percent escapes expanded */
      char
        key[MagickPathExtent];

      Image
        *write_images;

      ImageInfo
        *write_info;

      /* Need images, unless a "null:" output coder is used */
      if ( _images == (Image *) NULL ) {
        if ( LocaleCompare(arg1,"null:") == 0 )
          break;
        CLIWandExceptArgBreak(OptionError,"NoImagesForWrite",option,arg1);
      }

      (void) FormatLocaleString(key,MagickPathExtent,"cache:%s",arg1);
      (void) DeleteImageRegistry(key);
      write_images=CloneImageList(_images,_exception);
      write_info=CloneImageInfo(_image_info);
      (void) WriteImages(write_info,write_images,arg1,_exception);
      write_info=DestroyImageInfo(write_info);
      write_images=DestroyImageList(write_images);
      break;
    }
    /*
      Parenthesis and Brace operations
    */
    if (LocaleCompare("(",option) == 0) {
      /* stack 'push' images */
      Stack
        *node;

      size_t
        size;

      size=0;
      node=cli_wand->image_list_stack;
      for ( ; node != (Stack *) NULL; node=node->next)
        size++;
      if ( size >= MAX_STACK_DEPTH )
        CLIWandExceptionBreak(OptionError,"ParenthesisNestedTooDeeply",option);
      node=(Stack *) AcquireMagickMemory(sizeof(*node));
      if (node == (Stack *) NULL)
        CLIWandExceptionBreak(ResourceLimitFatalError,
            "MemoryAllocationFailed",option);
      node->data = (void *)cli_wand->wand.images;
      node->next = cli_wand->image_list_stack;
      cli_wand->image_list_stack = node;
      cli_wand->wand.images = NewImageList();

      /* handle respect-parenthesis */
      if (IsStringTrue(GetImageOption(cli_wand->wand.image_info,
                    "respect-parenthesis")) != MagickFalse)
        option="{"; /* fall-thru so as to push image settings too */
      else
        break;
      /* fall thru to operation */
    }
    if (LocaleCompare("{",option) == 0) {
      /* stack 'push' of image_info settings */
      Stack
        *node;

      size_t
        size;

      size=0;
      node=cli_wand->image_info_stack;
      for ( ; node != (Stack *) NULL; node=node->next)
        size++;
      if ( size >= MAX_STACK_DEPTH )
        CLIWandExceptionBreak(OptionError,"CurlyBracesNestedTooDeeply",option);
      node=(Stack *) AcquireMagickMemory(sizeof(*node));
      if (node == (Stack *) NULL)
        CLIWandExceptionBreak(ResourceLimitFatalError,
            "MemoryAllocationFailed",option);

      node->data = (void *)cli_wand->wand.image_info;
      node->next = cli_wand->image_info_stack;

      cli_wand->image_info_stack = node;
      cli_wand->wand.image_info = CloneImageInfo(cli_wand->wand.image_info);
      if (cli_wand->wand.image_info == (ImageInfo *) NULL) {
        CLIWandException(ResourceLimitFatalError,"MemoryAllocationFailed",
            option);
        cli_wand->wand.image_info = (ImageInfo *)node->data;
        node = (Stack *)RelinquishMagickMemory(node);
        break;
      }

      break;
    }
    if (LocaleCompare(")",option) == 0) {
      /* pop images from stack */
      Stack
        *node;

      node = (Stack *)cli_wand->image_list_stack;
      if ( node == (Stack *) NULL)
        CLIWandExceptionBreak(OptionError,"UnbalancedParenthesis",option);
      cli_wand->image_list_stack = node->next;

      AppendImageToList((Image **)&node->data,cli_wand->wand.images);
      cli_wand->wand.images= (Image *)node->data;
      node = (Stack *)RelinquishMagickMemory(node);

      /* handle respect-parenthesis - of the previous 'pushed' settings */
      node = cli_wand->image_info_stack;
      if ( node != (Stack *) NULL)
        {
          if (IsStringTrue(GetImageOption(
                cli_wand->wand.image_info,"respect-parenthesis")) != MagickFalse)
            option="}"; /* fall-thru so as to pop image settings too */
          else
            break;
        }
      else
        break;
      /* fall thru to next if */
    }
    if (LocaleCompare("}",option) == 0) {
      /* pop image_info settings from stack */
      Stack
        *node;

      node = (Stack *)cli_wand->image_info_stack;
      if ( node == (Stack *) NULL)
        CLIWandExceptionBreak(OptionError,"UnbalancedCurlyBraces",option);
      cli_wand->image_info_stack = node->next;

      (void) DestroyImageInfo(cli_wand->wand.image_info);
      cli_wand->wand.image_info = (ImageInfo *)node->data;
      node = (Stack *)RelinquishMagickMemory(node);

      GetDrawInfo(cli_wand->wand.image_info, cli_wand->draw_info);
      cli_wand->quantize_info=DestroyQuantizeInfo(cli_wand->quantize_info);
      cli_wand->quantize_info=AcquireQuantizeInfo(cli_wand->wand.image_info);

      break;
    }
      if (LocaleCompare("print",option+1) == 0)
        {
          (void) FormatLocaleFile(stdout,"%s",arg1);
          break;
        }
    if (LocaleCompare("set",option+1) == 0)
      {
        /* Settings are applied to each image in memory in turn (if any).
           While a option: only need to be applied once globally.

           NOTE: rguments have not been automatically percent expaneded
        */

        /* escape the 'key' once only, using first image. */
        arg1=InterpretImageProperties(_image_info,_images,arg1n,_exception);
        if (arg1 == (char *) NULL)
          CLIWandExceptionBreak(OptionWarning,"InterpretPropertyFailure",
                option);

        if (LocaleNCompare(arg1,"registry:",9) == 0)
          {
            if (IfPlusOp)
              {
                (void) DeleteImageRegistry(arg1+9);
                arg1=DestroyString((char *)arg1);
                break;
              }
            arg2=InterpretImageProperties(_image_info,_images,arg2n,_exception);
            if (arg2 == (char *) NULL) {
              arg1=DestroyString((char *)arg1);
              CLIWandExceptionBreak(OptionWarning,"InterpretPropertyFailure",
                    option);
            }
            (void) SetImageRegistry(StringRegistryType,arg1+9,arg2,_exception);
            arg1=DestroyString((char *)arg1);
            arg2=DestroyString((char *)arg2);
            break;
          }
        if (LocaleNCompare(arg1,"option:",7) == 0)
          {
            /* delete equivelent artifact from all images (if any) */
            if (_images != (Image *) NULL)
              {
                MagickResetIterator(&cli_wand->wand);
                while (MagickNextImage(&cli_wand->wand) != MagickFalse)
                  (void) DeleteImageArtifact(_images,arg1+7);
                MagickResetIterator(&cli_wand->wand);
              }
            /* now set/delete the global option as needed */
            /* FUTURE: make escapes in a global 'option:' delayed */
            arg2=(char *) NULL;
            if (IfNormalOp)
              {
                arg2=InterpretImageProperties(_image_info,_images,arg2n,_exception);
                if (arg2 == (char *) NULL)
                  CLIWandExceptionBreak(OptionWarning,
                       "InterpretPropertyFailure",option);
              }
            (void) SetImageOption(_image_info,arg1+7,arg2);
            arg1=DestroyString((char *)arg1);
            arg2=DestroyString((char *)arg2);
            break;
          }
        /* Set Artifacts/Properties/Attributes all images (required) */
        if ( _images == (Image *) NULL )
          CLIWandExceptArgBreak(OptionWarning,"NoImageForProperty",option,arg1);

        MagickResetIterator(&cli_wand->wand);
        while (MagickNextImage(&cli_wand->wand) != MagickFalse)
          {
            arg2=(char *) NULL;
            if (IfNormalOp)
              {
                arg2=InterpretImageProperties(_image_info,_images,arg2n,_exception);
                if (arg2 == (char *) NULL)
                  CLIWandExceptionBreak(OptionWarning,
                       "InterpretPropertyFailure",option);
              }
            if (LocaleNCompare(arg1,"artifact:",9) == 0)
              (void) SetImageArtifact(_images,arg1+9,arg2);
            else if (LocaleNCompare(arg1,"property:",9) == 0)
              (void) SetImageProperty(_images,arg1+9,arg2,_exception);
            else
              (void) SetImageProperty(_images,arg1,arg2,_exception);
            arg2=DestroyString((char *)arg2);
          }
        MagickResetIterator(&cli_wand->wand);
        arg1=DestroyString((char *)arg1);
        break;
     }
    if (LocaleCompare("clone",option+1) == 0) {
        Image
          *new_images;

        if (*option == '+')
          arg1=AcquireString("-1");
        if (IsSceneGeometry(arg1,MagickFalse) == MagickFalse)
          CLIWandExceptionBreak(OptionError,"InvalidArgument",option);
        if ( cli_wand->image_list_stack == (Stack *) NULL)
          CLIWandExceptionBreak(OptionError,"UnableToCloneImage",option);
        new_images = (Image *)cli_wand->image_list_stack->data;
        if (new_images == (Image *) NULL)
          CLIWandExceptionBreak(OptionError,"UnableToCloneImage",option);
        new_images=CloneImages(new_images,arg1,_exception);
        if (new_images == (Image *) NULL)
          CLIWandExceptionBreak(OptionError,"NoSuchImage",option);
        AppendImageToList(&_images,new_images);
        break;
      }
    /*
       Informational Operations.

       Note that these do not require either a cli-wand or images!
       Though currently a cli-wand much be provided regardless.
    */
    if (LocaleCompare("version",option+1) == 0)
      {
        ListMagickVersion(stdout);
        break;
      }
    if (LocaleCompare("list",option+1) == 0) {
      /*
         FUTURE: This 'switch' should really be part of MagickCore
      */
      ssize_t
        list;

      list=ParseCommandOption(MagickListOptions,MagickFalse,arg1);
      if ( list < 0 ) {
        CLIWandExceptionArg(OptionError,"UnrecognizedListType",option,arg1);
        break;
      }
      switch (list)
      {
        case MagickCoderOptions:
        {
          (void) ListCoderInfo((FILE *) NULL,_exception);
          break;
        }
        case MagickColorOptions:
        {
          (void) ListColorInfo((FILE *) NULL,_exception);
          break;
        }
        case MagickConfigureOptions:
        {
          (void) ListConfigureInfo((FILE *) NULL,_exception);
          break;
        }
        case MagickDelegateOptions:
        {
          (void) ListDelegateInfo((FILE *) NULL,_exception);
          break;
        }
        case MagickFontOptions:
        {
          (void) ListTypeInfo((FILE *) NULL,_exception);
          break;
        }
        case MagickFormatOptions:
          (void) ListMagickInfo((FILE *) NULL,_exception);
          break;
        case MagickLocaleOptions:
          (void) ListLocaleInfo((FILE *) NULL,_exception);
          break;
        case MagickLogOptions:
          (void) ListLogInfo((FILE *) NULL,_exception);
          break;
        case MagickMagicOptions:
          (void) ListMagicInfo((FILE *) NULL,_exception);
          break;
        case MagickMimeOptions:
          (void) ListMimeInfo((FILE *) NULL,_exception);
          break;
        case MagickModuleOptions:
          (void) ListModuleInfo((FILE *) NULL,_exception);
          break;
        case MagickPolicyOptions:
          (void) ListPolicyInfo((FILE *) NULL,_exception);
          break;
        case MagickResourceOptions:
          (void) ListMagickResourceInfo((FILE *) NULL,_exception);
          break;
        case MagickThresholdOptions:
          (void) ListThresholdMaps((FILE *) NULL,_exception);
          break;
        default:
          (void) ListCommandOptions((FILE *) NULL,(CommandOption) list,
            _exception);
          break;
      }
      break;
    }

    CLIWandException(OptionError,"UnrecognizedOption",option);

DisableMSCWarning(4127)
  } while (0);  /* break to exit code. */
RestoreMSCWarning

  /* clean up percent escape interpreted strings */
  if (arg1 != arg1n )
    arg1=DestroyString((char *)arg1);
  if (arg2 != arg2n )
    arg2=DestroyString((char *)arg2);

#undef _image_info
#undef _images
#undef _exception
#undef IfNormalOp
#undef IfPlusOp
}