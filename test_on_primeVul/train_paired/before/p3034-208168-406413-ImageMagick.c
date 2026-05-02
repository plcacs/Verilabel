static Image *XMagickCommand(Display *display,XResourceInfo *resource_info,
  XWindows *windows,const CommandType command_type,Image **image,
  MagickStatusType *state,ExceptionInfo *exception)
{
  Image
    *nexus;

  MagickBooleanType
    proceed;

  MagickStatusType
    status;

  XTextProperty
    window_name;

  /*
    Process user command.
  */
  nexus=NewImageList();
  switch (command_type)
  {
    case OpenCommand:
    {
      char
        **filelist;

      Image
        *images,
        *next;

      ImageInfo
        *read_info;

      int
        number_files;

      register int
        i;

      static char
        filenames[MagickPathExtent] = "*";

      if (resource_info->immutable != MagickFalse)
        break;
      /*
        Request file name from user.
      */
      XFileBrowserWidget(display,windows,"Animate",filenames);
      if (*filenames == '\0')
        return((Image *) NULL);
      /*
        Expand the filenames.
      */
      filelist=(char **) AcquireMagickMemory(sizeof(char *));
      if (filelist == (char **) NULL)
        {
          ThrowXWindowException(ResourceLimitError,"MemoryAllocationFailed",
            filenames);
          return((Image *) NULL);
        }
      number_files=1;
      filelist[0]=filenames;
      status=ExpandFilenames(&number_files,&filelist);
      if ((status == MagickFalse) || (number_files == 0))
        {
          if (number_files == 0)
            {
              ThrowXWindowException(ImageError,"NoImagesWereLoaded",filenames);
             return((Image *) NULL);
            }
          ThrowXWindowException(ResourceLimitError,"MemoryAllocationFailed",
            filenames);
          return((Image *) NULL);
        }
      read_info=CloneImageInfo(resource_info->image_info);
      images=NewImageList();
      XSetCursorState(display,windows,MagickTrue);
      XCheckRefreshWindows(display,windows);
      for (i=0; i < number_files; i++)
      {
        (void) CopyMagickString(read_info->filename,filelist[i],MagickPathExtent);
        filelist[i]=DestroyString(filelist[i]);
        *read_info->magick='\0';
        next=ReadImage(read_info,exception);
        CatchException(exception);
        if (next != (Image *) NULL)
          AppendImageToList(&images,next);
        if (number_files <= 5)
          continue;
        proceed=SetImageProgress(images,LoadImageTag,i,(MagickSizeType)
          number_files);
        if (proceed == MagickFalse)
          break;
      }
      filelist=(char **) RelinquishMagickMemory(filelist);
      read_info=DestroyImageInfo(read_info);
      if (images == (Image *) NULL)
        {
          XSetCursorState(display,windows,MagickFalse);
          ThrowXWindowException(ImageError,"NoImagesWereLoaded",filenames);
          return((Image *) NULL);
        }
      nexus=GetFirstImageInList(images);
      *state|=ExitState;
      break;
    }
    case PlayCommand:
    {
      char
        basename[MagickPathExtent];

      int
        status;

      /*
        Window name is the base of the filename.
      */
      *state|=PlayAnimationState;
      *state&=(~AutoReverseAnimationState);
      GetPathComponent((*image)->magick_filename,BasePath,basename);
      (void) FormatLocaleString(windows->image.name,MagickPathExtent,
        "%s: %s",MagickPackageName,basename);
      if (resource_info->title != (char *) NULL)
        {
          char
            *title;

          title=InterpretImageProperties(resource_info->image_info,*image,
            resource_info->title,exception);
          (void) CopyMagickString(windows->image.name,title,MagickPathExtent);
          title=DestroyString(title);
        }
      status=XStringListToTextProperty(&windows->image.name,1,&window_name);
      if (status == 0)
        break;
      XSetWMName(display,windows->image.id,&window_name);
      (void) XFree((void *) window_name.value);
      break;
    }
    case StepCommand:
    case StepBackwardCommand:
    case StepForwardCommand:
    {
      *state|=StepAnimationState;
      *state&=(~PlayAnimationState);
      if (command_type == StepBackwardCommand)
        *state&=(~ForwardAnimationState);
      if (command_type == StepForwardCommand)
        *state|=ForwardAnimationState;
      if (resource_info->title != (char *) NULL)
        break;
      break;
    }
    case RepeatCommand:
    {
      *state|=RepeatAnimationState;
      *state&=(~AutoReverseAnimationState);
      *state|=PlayAnimationState;
      break;
    }
    case AutoReverseCommand:
    {
      *state|=AutoReverseAnimationState;
      *state&=(~RepeatAnimationState);
      *state|=PlayAnimationState;
      break;
    }
    case SaveCommand:
    {
      /*
        Save image.
      */
      status=XSaveImage(display,resource_info,windows,*image,exception);
      if (status == MagickFalse)
        {
          char
            message[MagickPathExtent];

          (void) FormatLocaleString(message,MagickPathExtent,"%s:%s",
            exception->reason != (char *) NULL ? exception->reason : "",
            exception->description != (char *) NULL ? exception->description :
            "");
          XNoticeWidget(display,windows,"Unable to save file:",message);
          break;
        }
      break;
    }
    case SlowerCommand:
    {
      resource_info->delay++;
      break;
    }
    case FasterCommand:
    {
      if (resource_info->delay == 0)
        break;
      resource_info->delay--;
      break;
    }
    case ForwardCommand:
    {
      *state=ForwardAnimationState;
      *state&=(~AutoReverseAnimationState);
      break;
    }
    case ReverseCommand:
    {
      *state&=(~ForwardAnimationState);
      *state&=(~AutoReverseAnimationState);
      break;
    }
    case InfoCommand:
    {
      XDisplayImageInfo(display,resource_info,windows,(Image *) NULL,*image,
        exception);
      break;
    }
    case HelpCommand:
    {
      /*
        User requested help.
      */
      XTextViewWidget(display,resource_info,windows,MagickFalse,
        "Help Viewer - Animate",AnimateHelp);
      break;
    }
    case BrowseDocumentationCommand:
    {
      Atom
        mozilla_atom;

      Window
        mozilla_window,
        root_window;

      /*
        Browse the ImageMagick documentation.
      */
      root_window=XRootWindow(display,XDefaultScreen(display));
      mozilla_atom=XInternAtom(display,"_MOZILLA_VERSION",MagickFalse);
      mozilla_window=XWindowByProperty(display,root_window,mozilla_atom);
      if (mozilla_window != (Window) NULL)
        {
          char
            command[MagickPathExtent],
            *url;

          /*
            Display documentation using Netscape remote control.
          */
          url=GetMagickHomeURL();
          (void) FormatLocaleString(command,MagickPathExtent,
            "openurl(%s,new-tab)",url);
          url=DestroyString(url);
          mozilla_atom=XInternAtom(display,"_MOZILLA_COMMAND",MagickFalse);
          (void) XChangeProperty(display,mozilla_window,mozilla_atom,
            XA_STRING,8,PropModeReplace,(unsigned char *) command,
            (int) strlen(command));
          XSetCursorState(display,windows,MagickFalse);
          break;
        }
      XSetCursorState(display,windows,MagickTrue);
      XCheckRefreshWindows(display,windows);
      status=InvokeDelegate(resource_info->image_info,*image,"browse",
        (char *) NULL,exception);
      if (status == MagickFalse)
        XNoticeWidget(display,windows,"Unable to browse documentation",
          (char *) NULL);
      XDelay(display,1500);
      XSetCursorState(display,windows,MagickFalse);
      break;
    }
    case VersionCommand:
    {
      XNoticeWidget(display,windows,GetMagickVersion((size_t *) NULL),
        GetMagickCopyright());
      break;
    }
    case QuitCommand:
    {
      /*
        exit program
      */
      if (resource_info->confirm_exit == MagickFalse)
        XClientMessage(display,windows->image.id,windows->im_protocols,
          windows->im_exit,CurrentTime);
      else
        {
          int
            status;

          /*
            Confirm program exit.
          */
          status=XConfirmWidget(display,windows,"Do you really want to exit",
            resource_info->client_name);
          if (status != 0)
            XClientMessage(display,windows->image.id,windows->im_protocols,
              windows->im_exit,CurrentTime);
        }
      break;
    }
    default:
      break;
  }
  return(nexus);
}