load_image (const gchar  *filename,
            GError      **error)
{
  FILE         *fd;
  GimpDrawable *drawable;
  GimpPixelRgn  pixel_rgn;
  guint16       offset_x, offset_y, bytesperline;
  gint32        width, height;
  gint32        image, layer;
  guchar       *dest, cmap[768];
  guint8        header_buf[128];

  fd = g_fopen (filename, "rb");

  if (! fd)
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                   _("Could not open '%s' for reading: %s"),
                   gimp_filename_to_utf8 (filename), g_strerror (errno));
      return -1;
    }

  gimp_progress_init_printf (_("Opening '%s'"),
                             gimp_filename_to_utf8 (filename));

  if (fread (header_buf, 128, 1, fd) == 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("Could not read header from '%s'"),
                   gimp_filename_to_utf8 (filename));
      return -1;
    }

  pcx_header_from_buffer (header_buf);

  if (pcx_header.manufacturer != 10)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   _("'%s' is not a PCX file"),
                   gimp_filename_to_utf8 (filename));
      return -1;
    }

  offset_x     = GUINT16_FROM_LE (pcx_header.x1);
  offset_y     = GUINT16_FROM_LE (pcx_header.y1);
  width        = GUINT16_FROM_LE (pcx_header.x2) - offset_x + 1;
  height       = GUINT16_FROM_LE (pcx_header.y2) - offset_y + 1;
  bytesperline = GUINT16_FROM_LE (pcx_header.bytesperline);

  if ((width < 0) || (width > GIMP_MAX_IMAGE_SIZE))
    {
      g_message (_("Unsupported or invalid image width: %d"), width);
      return -1;
    }
  if ((height < 0) || (height > GIMP_MAX_IMAGE_SIZE))
    {
      g_message (_("Unsupported or invalid image height: %d"), height);
      return -1;
    }
  if (bytesperline < (width * pcx_header.bpp) / 8)
    {
      g_message (_("Invalid number of bytes per line in PCX header"));
      return -1;
    }

  /* Shield against potential buffer overflows in load_*() functions. */
  if (G_MAXSIZE / width / height < 3)
    {
      g_message (_("Image dimensions too large: width %d x height %d"), width, height);
      return -1;
    }

  if (pcx_header.planes == 3 && pcx_header.bpp == 8)
    {
      image= gimp_image_new (width, height, GIMP_RGB);
      layer= gimp_layer_new (image, _("Background"), width, height,
                             GIMP_RGB_IMAGE, 100, GIMP_NORMAL_MODE);
    }
  else
    {
      image= gimp_image_new (width, height, GIMP_INDEXED);
      layer= gimp_layer_new (image, _("Background"), width, height,
                             GIMP_INDEXED_IMAGE, 100, GIMP_NORMAL_MODE);
    }
  gimp_image_set_filename (image, filename);
  gimp_image_add_layer (image, layer, 0);
  gimp_layer_set_offsets (layer, offset_x, offset_y);
  drawable = gimp_drawable_get (layer);

  if (pcx_header.planes == 1 && pcx_header.bpp == 1)
    {
      dest = g_new (guchar, width * height);
      load_1 (fd, width, height, dest, bytesperline);
      gimp_image_set_colormap (image, mono, 2);
    }
  else if (pcx_header.planes == 4 && pcx_header.bpp == 1)
    {
      dest = g_new (guchar, width * height);
      load_4 (fd, width, height, dest, bytesperline);
      gimp_image_set_colormap (image, pcx_header.colormap, 16);
    }
  else if (pcx_header.planes == 1 && pcx_header.bpp == 8)
    {
      dest = g_new (guchar, width * height);
      load_8 (fd, width, height, dest, bytesperline);
      fseek (fd, -768L, SEEK_END);
      fread (cmap, 768, 1, fd);
      gimp_image_set_colormap (image, cmap, 256);
    }
  else if (pcx_header.planes == 3 && pcx_header.bpp == 8)
    {
      dest = g_new (guchar, width * height * 3);
      load_24 (fd, width, height, dest, bytesperline);
    }
  else
    {
      g_message (_("Unusual PCX flavour, giving up"));
      return -1;
    }

  gimp_pixel_rgn_init (&pixel_rgn, drawable, 0, 0, width, height, TRUE, FALSE);
  gimp_pixel_rgn_set_rect (&pixel_rgn, dest, 0, 0, width, height);

  g_free (dest);

  gimp_drawable_flush (drawable);
  gimp_drawable_detach (drawable);

  return image;
}