ppm_load_read_header(FILE       *fp,
                     pnm_struct *img)
{
    /* PPM Headers Variable Declaration */
    gchar *ptr;
    //gchar *retval;
    gchar  header[MAX_CHARS_IN_ROW];
    gint   maxval;
    int    channel_count;

    /* Check the PPM file Type P3 or P6 */
    if (fgets (header, MAX_CHARS_IN_ROW, fp) == NULL ||
        header[0] != ASCII_P ||
        (header[1] != PIXMAP_ASCII_GRAY &&
         header[1] != PIXMAP_ASCII &&
         header[1] != PIXMAP_RAW_GRAY &&
         header[1] != PIXMAP_RAW))
      {
        g_warning ("Image is not a portable pixmap");
        return FALSE;
      }

    img->type = header[1];

    if (img->type == PIXMAP_RAW_GRAY || img->type == PIXMAP_ASCII_GRAY)
      channel_count = CHANNEL_COUNT_GRAY;
    else
      channel_count = CHANNEL_COUNT;

    /* Check the Comments */
    while((fgets (header, MAX_CHARS_IN_ROW, fp)) && (header[0] == '#'))
      ;

    /* Get Width and Height */
    errno = 0;
    img->width = strtol (header, &ptr, 10);
    if (errno)
      {
        g_warning ("Error reading width: %s", strerror(errno));
        return FALSE;
      }
    else if (img->width < 0)
      {
        g_warning ("Error: width is negative");
        return FALSE;
      }

    img->height = strtol (ptr, &ptr, 10);
    if (errno)
      {
        g_warning ("Error reading height: %s", strerror(errno));
        return FALSE;
      }
    else if (img->width < 0)
      {
        g_warning ("Error: height is negative");
        return FALSE;
      }

    if (fgets (header, MAX_CHARS_IN_ROW, fp))
      maxval = strtol (header, &ptr, 10);
    else
      maxval = 0;

    if ((maxval != 255) && (maxval != 65535))
      {
        g_warning ("Image is not an 8-bit or 16-bit portable pixmap");
        return FALSE;
      }

  switch (maxval)
    {
    case 255:
      img->bpc = sizeof (guchar);
      break;

    case 65535:
      img->bpc = sizeof (gushort);
      break;

    default:
      g_warning ("%s: Programmer stupidity error", G_STRLOC);
    }

    /* Later on, img->numsamples is multiplied with img->bpc to allocate
     * memory. Ensure it doesn't overflow. */
    if (!img->width || !img->height ||
        G_MAXSIZE / img->width / img->height / CHANNEL_COUNT < img->bpc)
      {
        g_warning ("Illegal width/height: %ld/%ld", img->width, img->height);
        return FALSE;
      }


    img->channels = channel_count;
    img->numsamples = img->width * img->height * channel_count;

    return TRUE;
}