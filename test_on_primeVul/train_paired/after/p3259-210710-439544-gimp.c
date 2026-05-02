xcf_load_stream (Gimp          *gimp,
                 GInputStream  *input,
                 GFile         *input_file,
                 GimpProgress  *progress,
                 GError       **error)
{
  XcfInfo      info  = { 0, };
  const gchar *filename;
  GimpImage   *image = NULL;
  gchar        id[14];
  gboolean     success;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);
  g_return_val_if_fail (G_IS_INPUT_STREAM (input), NULL);
  g_return_val_if_fail (input_file == NULL || G_IS_FILE (input_file), NULL);
  g_return_val_if_fail (progress == NULL || GIMP_IS_PROGRESS (progress), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (input_file)
    filename = gimp_file_get_utf8_name (input_file);
  else
    filename = _("Memory Stream");

  info.gimp             = gimp;
  info.input            = input;
  info.seekable         = G_SEEKABLE (input);
  info.bytes_per_offset = 4;
  info.progress         = progress;
  info.file             = input_file;
  info.compression      = COMPRESS_NONE;

  if (progress)
    gimp_progress_start (progress, FALSE, _("Opening '%s'"), filename);

  success = TRUE;

  xcf_read_int8 (&info, (guint8 *) id, 14);

  if (! g_str_has_prefix (id, "gimp xcf "))
    {
      success = FALSE;
    }
  else if (strcmp (id + 9, "file") == 0)
    {
      info.file_version = 0;
    }
  else if (id[9]  == 'v' &&
           id[13] == '\0')
    {
      info.file_version = atoi (id + 10);
    }
  else
    {
      success = FALSE;
    }

  if (info.file_version >= 11)
    info.bytes_per_offset = 8;

  if (success)
    {
      if (info.file_version >= 0 &&
          info.file_version < G_N_ELEMENTS (xcf_loaders))
        {
          image = (*(xcf_loaders[info.file_version])) (gimp, &info, error);

          if (! image)
            success = FALSE;

          g_input_stream_close (info.input, NULL, NULL);
        }
      else
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       _("XCF error: unsupported XCF file version %d "
                         "encountered"), info.file_version);
          success = FALSE;
        }
    }

  if (progress)
    gimp_progress_end (progress);

  return image;
}