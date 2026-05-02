load_image (const gchar  *filename,
            GError      **error)
{
  gchar             *name;
  gint               fd;
  BrushHeader        bh;
  guchar            *brush_buf = NULL;
  gint32             image_ID;
  gint32             layer_ID;
  GimpParasite      *parasite;
  GimpDrawable      *drawable;
  GimpPixelRgn       pixel_rgn;
  gint               bn_size;
  GimpImageBaseType  base_type;
  GimpImageType      image_type;
  gsize              size;

  fd = g_open (filename, O_RDONLY | _O_BINARY, 0);

  if (fd == -1)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Could not open '%s' for reading: %s"),
                   gimp_filename_to_utf8 (filename), g_strerror (errno));
      return -1;
    }

  gimp_progress_init_printf (_("Opening '%s'"),
                             gimp_filename_to_utf8 (filename));

  if (read (fd, &bh, sizeof (BrushHeader)) != sizeof (BrushHeader))
    {
      close (fd);
      return -1;
    }

  /*  rearrange the bytes in each unsigned int  */
  bh.header_size  = g_ntohl (bh.header_size);
  bh.version      = g_ntohl (bh.version);
  bh.width        = g_ntohl (bh.width);
  bh.height       = g_ntohl (bh.height);
  bh.bytes        = g_ntohl (bh.bytes);
  bh.magic_number = g_ntohl (bh.magic_number);
  bh.spacing      = g_ntohl (bh.spacing);

  /* Sanitize values */
  if ((bh.width == 0) || (bh.width > GIMP_MAX_IMAGE_SIZE) ||
      (bh.height == 0) || (bh.height > GIMP_MAX_IMAGE_SIZE) ||
      ((bh.bytes != 1) && (bh.bytes != 2) && (bh.bytes != 4) &&
       (bh.bytes != 18)) ||
      (G_MAXSIZE / bh.width / bh.height / bh.bytes < 1))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Invalid header data in '%s': width=%lu, height=%lu, "
                     "bytes=%lu"), gimp_filename_to_utf8 (filename),
                   (unsigned long int)bh.width, (unsigned long int)bh.height,
                   (unsigned long int)bh.bytes);
      return -1;
    }

  switch (bh.version)
    {
    case 1:
      /* Version 1 didn't have a magic number and had no spacing  */
      bh.spacing = 25;
      /* And we need to rewind the handle, 4 due spacing and 4 due magic */
      lseek (fd, -8, SEEK_CUR);
      bh.header_size += 8;
      break;

    case 3: /*  cinepaint brush  */
      if (bh.bytes == 18 /* FLOAT16_GRAY_GIMAGE */)
        {
          bh.bytes = 2;
        }
      else
        {
          g_message (_("Unsupported brush format"));
          close (fd);
          return -1;
        }
      /*  fallthrough  */

    case 2:
      if (bh.magic_number == GBRUSH_MAGIC &&
          bh.header_size  >  sizeof (BrushHeader))
        break;

    default:
      g_message (_("Unsupported brush format"));
      close (fd);
      return -1;
    }

  if ((bn_size = (bh.header_size - sizeof (BrushHeader))) > 0)
    {
      gchar *temp = g_new (gchar, bn_size);

      if ((read (fd, temp, bn_size)) < bn_size)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("Error in GIMP brush file '%s'"),
                       gimp_filename_to_utf8 (filename));
          close (fd);
          g_free (temp);
          return -1;
        }

      name = gimp_any_to_utf8 (temp, -1,
                               _("Invalid UTF-8 string in brush file '%s'."),
                               gimp_filename_to_utf8 (filename));
      g_free (temp);
    }
  else
    {
      name = g_strdup (_("Unnamed"));
    }

  /* Now there's just raw data left. */

  size = bh.width * bh.height * bh.bytes;
  brush_buf = g_malloc (size);

  if (read (fd, brush_buf, size) != size)
    {
      close (fd);
      g_free (brush_buf);
      g_free (name);
      return -1;
    }

  switch (bh.bytes)
    {
    case 1:
      {
        PatternHeader ph;

        /*  For backwards-compatibility, check if a pattern follows.
            The obsolete .gpb format did it this way.  */

        if (read (fd, &ph, sizeof (PatternHeader)) == sizeof(PatternHeader))
          {
            /*  rearrange the bytes in each unsigned int  */
            ph.header_size  = g_ntohl (ph.header_size);
            ph.version      = g_ntohl (ph.version);
            ph.width        = g_ntohl (ph.width);
            ph.height       = g_ntohl (ph.height);
            ph.bytes        = g_ntohl (ph.bytes);
            ph.magic_number = g_ntohl (ph.magic_number);

            if (ph.magic_number == GPATTERN_MAGIC        &&
                ph.version      == 1                     &&
                ph.header_size  > sizeof (PatternHeader) &&
                ph.bytes        == 3                     &&
                ph.width        == bh.width              &&
                ph.height       == bh.height             &&
                lseek (fd, ph.header_size - sizeof (PatternHeader),
                       SEEK_CUR) > 0)
              {
                guchar *plain_brush = brush_buf;
                gint    i;

                bh.bytes = 4;
                brush_buf = g_malloc (4 * bh.width * bh.height);

                for (i = 0; i < ph.width * ph.height; i++)
                  {
                    if (read (fd, brush_buf + i * 4, 3) != 3)
                      {
                        close (fd);
                        g_free (name);
                        g_free (plain_brush);
                        g_free (brush_buf);
                        return -1;
                      }
                    brush_buf[i * 4 + 3] = plain_brush[i];
                  }
                g_free (plain_brush);
              }
          }
      }
      break;

    case 2:
      {
        guint16 *buf = (guint16 *) brush_buf;
        gint     i;

        for (i = 0; i < bh.width * bh.height; i++, buf++)
          {
            union
            {
              guint16 u[2];
              gfloat  f;
            } short_float;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
            short_float.u[0] = 0;
            short_float.u[1] = GUINT16_FROM_BE (*buf);
#else
            short_float.u[0] = GUINT16_FROM_BE (*buf);
            short_float.u[1] = 0;
#endif

            brush_buf[i] = (guchar) (short_float.f * 255.0 + 0.5);
          }

        bh.bytes = 1;
      }
      break;

    default:
      break;
    }

  /*
   * Create a new image of the proper size and
   * associate the filename with it.
   */

  switch (bh.bytes)
    {
    case 1:
      base_type = GIMP_GRAY;
      image_type = GIMP_GRAY_IMAGE;
      break;

    case 4:
      base_type = GIMP_RGB;
      image_type = GIMP_RGBA_IMAGE;
      break;

    default:
      g_message ("Unsupported brush depth: %d\n"
                 "GIMP Brushes must be GRAY or RGBA\n",
                 bh.bytes);
      g_free (name);
      return -1;
    }

  image_ID = gimp_image_new (bh.width, bh.height, base_type);
  gimp_image_set_filename (image_ID, filename);

  parasite = gimp_parasite_new ("gimp-brush-name",
                                GIMP_PARASITE_PERSISTENT,
                                strlen (name) + 1, name);
  gimp_image_attach_parasite (image_ID, parasite);
  gimp_parasite_free (parasite);

  layer_ID = gimp_layer_new (image_ID, name, bh.width, bh.height,
                             image_type, 100, GIMP_NORMAL_MODE);
  gimp_image_insert_layer (image_ID, layer_ID, -1, 0);

  g_free (name);

  drawable = gimp_drawable_get (layer_ID);
  gimp_pixel_rgn_init (&pixel_rgn, drawable,
                       0, 0, drawable->width, drawable->height,
                       TRUE, FALSE);

  gimp_pixel_rgn_set_rect (&pixel_rgn, brush_buf,
                           0, 0, bh.width, bh.height);
  g_free (brush_buf);

  if (image_type == GIMP_GRAY_IMAGE)
    gimp_invert (layer_ID);

  close (fd);

  gimp_drawable_flush (drawable);
  gimp_progress_update (1.0);

  return image_ID;
}