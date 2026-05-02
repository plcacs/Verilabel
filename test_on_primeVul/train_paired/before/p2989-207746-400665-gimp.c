load_image (GFile   *file,
            GError **error)
{
  GInputStream      *input;
  gchar             *name;
  BrushHeader        bh;
  guchar            *brush_buf = NULL;
  gint32             image_ID;
  gint32             layer_ID;
  GimpParasite      *parasite;
  GeglBuffer        *buffer;
  const Babl        *format;
  GimpImageBaseType  base_type;
  GimpImageType      image_type;
  gsize              bytes_read;
  gsize              size;
  gint               i;

  gimp_progress_init_printf (_("Opening '%s'"),
                             g_file_get_parse_name (file));

  input = G_INPUT_STREAM (g_file_read (file, NULL, error));
  if (! input)
    return -1;

  size = G_STRUCT_OFFSET (BrushHeader, magic_number);

  if (! g_input_stream_read_all (input, &bh, size,
                                 &bytes_read, NULL, error) ||
      bytes_read != size)
    {
      g_object_unref (input);
      return -1;
    }

  /*  rearrange the bytes in each unsigned int  */
  bh.header_size  = g_ntohl (bh.header_size);
  bh.version      = g_ntohl (bh.version);
  bh.width        = g_ntohl (bh.width);
  bh.height       = g_ntohl (bh.height);
  bh.bytes        = g_ntohl (bh.bytes);

  /* Sanitize values */
  if ((bh.width  == 0) || (bh.width  > GIMP_MAX_IMAGE_SIZE) ||
      (bh.height == 0) || (bh.height > GIMP_MAX_IMAGE_SIZE) ||
      ((bh.bytes != 1) && (bh.bytes != 2) && (bh.bytes != 4) &&
       (bh.bytes != 18)) ||
      (G_MAXSIZE / bh.width / bh.height / bh.bytes < 1))
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Invalid header data in '%s': width=%lu, height=%lu, "
                     "bytes=%lu"), g_file_get_parse_name (file),
                   (unsigned long int)bh.width, (unsigned long int)bh.height,
                   (unsigned long int)bh.bytes);
      return -1;
    }

  switch (bh.version)
    {
    case 1:
      /* Version 1 didn't have a magic number and had no spacing  */
      bh.spacing = 25;
      bh.header_size += 8;
      break;

    case 2:
    case 3: /*  cinepaint brush  */
      size = sizeof (bh.magic_number) + sizeof (bh.spacing);

      if (! g_input_stream_read_all (input,
                                     (guchar *) &bh +
                                     G_STRUCT_OFFSET (BrushHeader,
                                                      magic_number), size,
                                     &bytes_read, NULL, error) ||
          bytes_read != size)
        {
          g_object_unref (input);
          return -1;
        }

      bh.magic_number = g_ntohl (bh.magic_number);
      bh.spacing      = g_ntohl (bh.spacing);

      if (bh.version == 3)
        {
          if (bh.bytes == 18 /* FLOAT16_GRAY_GIMAGE */)
            {
              bh.bytes = 2;
            }
          else
            {
              g_message (_("Unsupported brush format"));
              g_object_unref (input);
              return -1;
            }
        }

      if (bh.magic_number == GBRUSH_MAGIC &&
          bh.header_size  >  sizeof (BrushHeader))
        break;

    default:
      g_message (_("Unsupported brush format"));
      g_object_unref (input);
      return -1;
    }

  if ((size = (bh.header_size - sizeof (BrushHeader))) > 0)
    {
      gchar *temp = g_new (gchar, size);

      if (! g_input_stream_read_all (input, temp, size,
                                     &bytes_read, NULL, error) ||
          bytes_read != size)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("Error in GIMP brush file '%s'"),
                       g_file_get_parse_name (file));
          g_object_unref (input);
          g_free (temp);
          return -1;
        }

      name = gimp_any_to_utf8 (temp, -1,
                               _("Invalid UTF-8 string in brush file '%s'."),
                               g_file_get_parse_name (file));
      g_free (temp);
    }
  else
    {
      name = g_strdup (_("Unnamed"));
    }

  /* Now there's just raw data left. */

  size = (gsize) bh.width * bh.height * bh.bytes;
  brush_buf = g_malloc (size);

  if (! g_input_stream_read_all (input, brush_buf, size,
                                 &bytes_read, NULL, error) ||
      bytes_read != size)
    {
      g_object_unref (input);
      g_free (brush_buf);
      g_free (name);
      return -1;
    }

  switch (bh.bytes)
    {
    case 1:
      {
        PatternHeader ph;

        /* For backwards-compatibility, check if a pattern follows.
         * The obsolete .gpb format did it this way.
         */

        if (g_input_stream_read_all (input, &ph, sizeof (PatternHeader),
                                     &bytes_read, NULL, NULL) &&
            bytes_read == sizeof (PatternHeader))
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
                g_input_stream_skip (input,
                                     ph.header_size - sizeof (PatternHeader),
                                     NULL, NULL) ==
                ph.header_size - sizeof (PatternHeader))
              {
                guchar *plain_brush = brush_buf;
                gint    i;

                bh.bytes = 4;
                brush_buf = g_malloc ((gsize) bh.width * bh.height * 4);

                for (i = 0; i < ph.width * ph.height; i++)
                  {
                    if (! g_input_stream_read_all (input,
                                                   brush_buf + i * 4, 3,
                                                   &bytes_read, NULL, error) ||
                        bytes_read != 3)
                      {
                        g_object_unref (input);
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
      format = babl_format ("Y' u8");
      break;

    case 4:
      base_type = GIMP_RGB;
      image_type = GIMP_RGBA_IMAGE;
      format = babl_format ("R'G'B'A u8");
      break;

    default:
      g_message ("Unsupported brush depth: %d\n"
                 "GIMP Brushes must be GRAY or RGBA\n",
                 bh.bytes);
      g_free (name);
      return -1;
    }

  image_ID = gimp_image_new (bh.width, bh.height, base_type);
  gimp_image_set_filename (image_ID, g_file_get_uri (file));

  parasite = gimp_parasite_new ("gimp-brush-name",
                                GIMP_PARASITE_PERSISTENT,
                                strlen (name) + 1, name);
  gimp_image_attach_parasite (image_ID, parasite);
  gimp_parasite_free (parasite);

  layer_ID = gimp_layer_new (image_ID, name, bh.width, bh.height,
                             image_type,
                             100,
                             gimp_image_get_default_new_layer_mode (image_ID));
  gimp_image_insert_layer (image_ID, layer_ID, -1, 0);

  g_free (name);

  buffer = gimp_drawable_get_buffer (layer_ID);

  /*  invert  */
  if (image_type == GIMP_GRAY_IMAGE)
    for (i = 0; i < bh.width * bh.height; i++)
      brush_buf[i] = 255 - brush_buf[i];

  gegl_buffer_set (buffer, GEGL_RECTANGLE (0, 0, bh.width, bh.height), 0,
                   format, brush_buf, GEGL_AUTO_ROWSTRIDE);

  g_free (brush_buf);
  g_object_unref (buffer);
  g_object_unref (input);

  gimp_progress_update (1.0);

  return image_ID;
}