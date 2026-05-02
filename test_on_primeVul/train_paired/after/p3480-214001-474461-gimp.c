load_image (const gchar  *filename,
            GError      **error)
{
  FILE            *ifp = NULL;
  gint             depth, bpp;
  gint32           image_ID = -1;
  L_XWDFILEHEADER  xwdhdr;
  L_XWDCOLOR      *xwdcolmap = NULL;

  ifp = g_fopen (filename, "rb");
  if (!ifp)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Could not open '%s' for reading: %s"),
                   gimp_filename_to_utf8 (filename), g_strerror (errno));
      goto out;
    }

  read_xwd_header (ifp, &xwdhdr);
  if (xwdhdr.l_file_version != 7)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Could not read XWD header from '%s'"),
                   gimp_filename_to_utf8 (filename));
      goto out;
    }

#ifdef XWD_COL_WAIT_DEBUG
  {
    int k = 1;

    while (k)
      k = k;
  }
#endif

  /* Position to start of XWDColor structures */
  fseek (ifp, (long)xwdhdr.l_header_size, SEEK_SET);

  /* Guard against insanely huge color maps -- gimp_image_set_colormap() only
   * accepts colormaps with 0..256 colors anyway. */
  if (xwdhdr.l_colormap_entries > 256)
    {
      g_message (_("'%s':\nIllegal number of colormap entries: %ld"),
                 gimp_filename_to_utf8 (filename),
                 (long)xwdhdr.l_colormap_entries);
      goto out;
    }

  if (xwdhdr.l_colormap_entries > 0)
    {
      if (xwdhdr.l_colormap_entries < xwdhdr.l_ncolors)
        {
          g_message (_("'%s':\nNumber of colormap entries < number of colors"),
                     gimp_filename_to_utf8 (filename));
          goto out;
        }

      xwdcolmap = g_new (L_XWDCOLOR, xwdhdr.l_colormap_entries);

      read_xwd_cols (ifp, &xwdhdr, xwdcolmap);

#ifdef XWD_COL_DEBUG
      {
        int j;
        printf ("File %s\n",filename);
        for (j = 0; j < xwdhdr.l_colormap_entries; j++)
          printf ("Entry 0x%08lx: 0x%04lx,  0x%04lx, 0x%04lx, %d\n",
                  (long)xwdcolmap[j].l_pixel,(long)xwdcolmap[j].l_red,
                  (long)xwdcolmap[j].l_green,(long)xwdcolmap[j].l_blue,
                  (int)xwdcolmap[j].l_flags);
      }
#endif

      if (xwdhdr.l_file_version != 7)
        {
          g_message (_("Can't read color entries"));
          goto out;
        }
    }

  if (xwdhdr.l_pixmap_width <= 0)
    {
      g_message (_("'%s':\nNo image width specified"),
                 gimp_filename_to_utf8 (filename));
      goto out;
    }

  if (xwdhdr.l_pixmap_width > GIMP_MAX_IMAGE_SIZE
      || xwdhdr.l_bytes_per_line > GIMP_MAX_IMAGE_SIZE * 3)
    {
      g_message (_("'%s':\nImage width is larger than GIMP can handle"),
                 gimp_filename_to_utf8 (filename));
      goto out;
    }

  if (xwdhdr.l_pixmap_height <= 0)
    {
      g_message (_("'%s':\nNo image height specified"),
                 gimp_filename_to_utf8 (filename));
      goto out;
    }

  if (xwdhdr.l_pixmap_height > GIMP_MAX_IMAGE_SIZE)
    {
      g_message (_("'%s':\nImage height is larger than GIMP can handle"),
                 gimp_filename_to_utf8 (filename));
      goto out;
    }

  gimp_progress_init_printf (_("Opening '%s'"),
                             gimp_filename_to_utf8 (filename));

  depth = xwdhdr.l_pixmap_depth;
  bpp   = xwdhdr.l_bits_per_pixel;

  image_ID = -1;
  switch (xwdhdr.l_pixmap_format)
    {
    case 0:    /* Single plane bitmap */
      if ((depth == 1) && (bpp == 1))
        { /* Can be performed by format 2 loader */
          image_ID = load_xwd_f2_d1_b1 (filename, ifp, &xwdhdr, xwdcolmap);
        }
      break;

    case 1:    /* Single plane pixmap */
      if ((depth <= 24) && (bpp == 1))
        {
          image_ID = load_xwd_f1_d24_b1 (filename, ifp, &xwdhdr, xwdcolmap,
                                         error);
        }
      break;

    case 2:    /* Multiplane pixmaps */
      if ((depth == 1) && (bpp == 1))
        {
          image_ID = load_xwd_f2_d1_b1 (filename, ifp, &xwdhdr, xwdcolmap);
        }
      else if ((depth <= 8) && (bpp == 8))
        {
          image_ID = load_xwd_f2_d8_b8 (filename, ifp, &xwdhdr, xwdcolmap);
        }
      else if ((depth <= 16) && (bpp == 16))
        {
          image_ID = load_xwd_f2_d16_b16 (filename, ifp, &xwdhdr, xwdcolmap);
        }
      else if ((depth <= 24) && ((bpp == 24) || (bpp == 32)))
        {
          image_ID = load_xwd_f2_d24_b32 (filename, ifp, &xwdhdr, xwdcolmap,
                                          error);
        }
      else if ((depth <= 32) && (bpp == 32))
        {
          image_ID = load_xwd_f2_d32_b32 (filename, ifp, &xwdhdr, xwdcolmap);
        }
      break;
    }
  gimp_progress_update (1.0);

  if (image_ID == -1 && ! (error && *error))
    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                 _("XWD-file %s has format %d, depth %d and bits per pixel %d. "
                   "Currently this is not supported."),
                 gimp_filename_to_utf8 (filename),
                 (gint) xwdhdr.l_pixmap_format, depth, bpp);

out:
  if (ifp)
    {
      fclose (ifp);
    }

  if (xwdcolmap)
    {
      g_free (xwdcolmap);
    }

  return image_ID;
}