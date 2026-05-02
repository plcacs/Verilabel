ppm_load_read_header(FILE       *fp,
                     pnm_struct *img)
{
    /* PPM Headers Variable Declaration */
    gchar *ptr;
    //gchar *retval;
    gchar  header[MAX_CHARS_IN_ROW];
    gint   maxval;

    /* Check the PPM file Type P2 or P5 */
    fgets (header,MAX_CHARS_IN_ROW,fp);

    if (header[0] != ASCII_P ||
        (header[1] != PIXMAP_ASCII &&
         header[1] != PIXMAP_RAW))
      {
        g_warning ("Image is not a portable pixmap");
        return FALSE;
      }

    img->type = header[1];

    /* Check the Comments */
    fgets (header,MAX_CHARS_IN_ROW,fp);
    while(header[0] == '#')
      {
        fgets (header,MAX_CHARS_IN_ROW,fp);
      }

    /* Get Width and Height */
    img->width  = strtol (header,&ptr,0);
    img->height = atoi (ptr);

    fgets (header,MAX_CHARS_IN_ROW,fp);
    maxval = strtol (header,&ptr,0);

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
    img->numsamples = img->width * img->height * CHANNEL_COUNT;

    return TRUE;
}