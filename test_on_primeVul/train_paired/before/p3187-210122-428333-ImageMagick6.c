MagickExport void DestroyXResources(void)
{
  register int
    i;

  unsigned int
    number_windows;

  XWindowInfo
    *magick_windows[MaxXWindows];

  XWindows
    *windows;

  DestroyXWidget();
  windows=XSetWindows((XWindows *) ~0);
  if ((windows == (XWindows *) NULL) || (windows->display == (Display *) NULL))
    return;
  number_windows=0;
  magick_windows[number_windows++]=(&windows->context);
  magick_windows[number_windows++]=(&windows->group_leader);
  magick_windows[number_windows++]=(&windows->backdrop);
  magick_windows[number_windows++]=(&windows->icon);
  magick_windows[number_windows++]=(&windows->image);
  magick_windows[number_windows++]=(&windows->info);
  magick_windows[number_windows++]=(&windows->magnify);
  magick_windows[number_windows++]=(&windows->pan);
  magick_windows[number_windows++]=(&windows->command);
  magick_windows[number_windows++]=(&windows->widget);
  magick_windows[number_windows++]=(&windows->popup);
  for (i=0; i < (int) number_windows; i++)
  {
    if (magick_windows[i]->mapped != MagickFalse)
      {
        (void) XWithdrawWindow(windows->display,magick_windows[i]->id,
          magick_windows[i]->screen);
        magick_windows[i]->mapped=MagickFalse;
      }
    if (magick_windows[i]->name != (char *) NULL)
      magick_windows[i]->name=(char *)
        RelinquishMagickMemory(magick_windows[i]->name);
    if (magick_windows[i]->icon_name != (char *) NULL)
      magick_windows[i]->icon_name=(char *)
        RelinquishMagickMemory(magick_windows[i]->icon_name);
    if (magick_windows[i]->cursor != (Cursor) NULL)
      {
        (void) XFreeCursor(windows->display,magick_windows[i]->cursor);
        magick_windows[i]->cursor=(Cursor) NULL;
      }
    if (magick_windows[i]->busy_cursor != (Cursor) NULL)
      {
        (void) XFreeCursor(windows->display,magick_windows[i]->busy_cursor);
        magick_windows[i]->busy_cursor=(Cursor) NULL;
      }
    if (magick_windows[i]->highlight_stipple != (Pixmap) NULL)
      {
        (void) XFreePixmap(windows->display,
          magick_windows[i]->highlight_stipple);
        magick_windows[i]->highlight_stipple=(Pixmap) NULL;
      }
    if (magick_windows[i]->shadow_stipple != (Pixmap) NULL)
      {
        (void) XFreePixmap(windows->display,magick_windows[i]->shadow_stipple);
        magick_windows[i]->shadow_stipple=(Pixmap) NULL;
      }
    if (magick_windows[i]->ximage != (XImage *) NULL)
      {
        XDestroyImage(magick_windows[i]->ximage);
        magick_windows[i]->ximage=(XImage *) NULL;
      }
    if (magick_windows[i]->pixmap != (Pixmap) NULL)
      {
        (void) XFreePixmap(windows->display,magick_windows[i]->pixmap);
        magick_windows[i]->pixmap=(Pixmap) NULL;
      }
    if (magick_windows[i]->id != (Window) NULL)
      {
        (void) XDestroyWindow(windows->display,magick_windows[i]->id);
        magick_windows[i]->id=(Window) NULL;
      }
    if (magick_windows[i]->destroy != MagickFalse)
      {
        if (magick_windows[i]->image != (Image *) NULL)
          {
            magick_windows[i]->image=DestroyImage(magick_windows[i]->image);
            magick_windows[i]->image=NewImageList();
          }
        if (magick_windows[i]->matte_pixmap != (Pixmap) NULL)
          {
            (void) XFreePixmap(windows->display,
              magick_windows[i]->matte_pixmap);
            magick_windows[i]->matte_pixmap=(Pixmap) NULL;
          }
      }
    if (magick_windows[i]->segment_info != (void *) NULL)
      {
#if defined(MAGICKCORE_HAVE_SHARED_MEMORY)
        XShmSegmentInfo
          *segment_info;

        segment_info=(XShmSegmentInfo *) magick_windows[i]->segment_info;
        if (segment_info != (XShmSegmentInfo *) NULL)
          if (segment_info[0].shmid >= 0)
            {
              if (segment_info[0].shmaddr != NULL)
                (void) shmdt(segment_info[0].shmaddr);
              (void) shmctl(segment_info[0].shmid,IPC_RMID,0);
              segment_info[0].shmaddr=NULL;
              segment_info[0].shmid=(-1);
            }
#endif
        magick_windows[i]->segment_info=(void *) RelinquishMagickMemory(
          magick_windows[i]->segment_info);
      }
  }
  windows->icon_resources=(XResourceInfo *)
    RelinquishMagickMemory(windows->icon_resources);
  if (windows->icon_pixel != (XPixelInfo *) NULL)
    {
      if (windows->icon_pixel->pixels != (unsigned long *) NULL)
        windows->icon_pixel->pixels=(unsigned long *)
          RelinquishMagickMemory(windows->icon_pixel->pixels);
      if (windows->icon_pixel->annotate_context != (GC) NULL)
        XFreeGC(windows->display,windows->icon_pixel->annotate_context);
      windows->icon_pixel=(XPixelInfo *)
        RelinquishMagickMemory(windows->icon_pixel);
    }
  if (windows->pixel_info != (XPixelInfo *) NULL)
    {
      if (windows->pixel_info->pixels != (unsigned long *) NULL)
        windows->pixel_info->pixels=(unsigned long *)
          RelinquishMagickMemory(windows->pixel_info->pixels);
      if (windows->pixel_info->annotate_context != (GC) NULL)
        XFreeGC(windows->display,windows->pixel_info->annotate_context);
      if (windows->pixel_info->widget_context != (GC) NULL)
        XFreeGC(windows->display,windows->pixel_info->widget_context);
      if (windows->pixel_info->highlight_context != (GC) NULL)
        XFreeGC(windows->display,windows->pixel_info->highlight_context);
      windows->pixel_info=(XPixelInfo *)
        RelinquishMagickMemory(windows->pixel_info);
    }
  if (windows->font_info != (XFontStruct *) NULL)
    {
      XFreeFont(windows->display,windows->font_info);
      windows->font_info=(XFontStruct *) NULL;
    }
  if (windows->class_hints != (XClassHint *) NULL)
    {
      if (windows->class_hints->res_name != (char *) NULL)
        windows->class_hints->res_name=DestroyString(
          windows->class_hints->res_name);
      if (windows->class_hints->res_class != (char *) NULL)
        windows->class_hints->res_class=DestroyString(
          windows->class_hints->res_class);
      XFree(windows->class_hints);
      windows->class_hints=(XClassHint *) NULL;
    }
  if (windows->manager_hints != (XWMHints *) NULL)
    {
      XFree(windows->manager_hints);
      windows->manager_hints=(XWMHints *) NULL;
    }
  if (windows->map_info != (XStandardColormap *) NULL)
    {
      XFree(windows->map_info);
      windows->map_info=(XStandardColormap *) NULL;
    }
  if (windows->icon_map != (XStandardColormap *) NULL)
    {
      XFree(windows->icon_map);
      windows->icon_map=(XStandardColormap *) NULL;
    }
  if (windows->visual_info != (XVisualInfo *) NULL)
    {
      XFree(windows->visual_info);
      windows->visual_info=(XVisualInfo *) NULL;
    }
  if (windows->icon_visual != (XVisualInfo *) NULL)
    {
      XFree(windows->icon_visual);
      windows->icon_visual=(XVisualInfo *) NULL;
    }
  (void) XSetWindows((XWindows *) NULL);
}