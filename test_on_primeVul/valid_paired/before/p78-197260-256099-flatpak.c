export_desktop_file (const char         *app,
                     const char         *branch,
                     const char         *arch,
                     GKeyFile           *metadata,
                     const char * const *previous_ids,
                     int                 parent_fd,
                     const char         *name,
                     struct stat        *stat_buf,
                     char              **target,
                     GCancellable       *cancellable,
                     GError            **error)
{
  gboolean ret = FALSE;
  glnx_autofd int desktop_fd = -1;
  g_autofree char *tmpfile_name = g_strdup_printf ("export-desktop-XXXXXX");
  g_autoptr(GOutputStream) out_stream = NULL;
  g_autofree gchar *data = NULL;
  gsize data_len;
  g_autofree gchar *new_data = NULL;
  gsize new_data_len;
  g_autoptr(GKeyFile) keyfile = NULL;
  g_autofree gchar *old_exec = NULL;
  gint old_argc;
  g_auto(GStrv) old_argv = NULL;
  g_auto(GStrv) groups = NULL;
  GString *new_exec = NULL;
  g_autofree char *escaped_app = maybe_quote (app);
  g_autofree char *escaped_branch = maybe_quote (branch);
  g_autofree char *escaped_arch = maybe_quote (arch);
  int i;

  if (!flatpak_openat_noatime (parent_fd, name, &desktop_fd, cancellable, error))
    goto out;

  if (!read_fd (desktop_fd, stat_buf, &data, &data_len, error))
    goto out;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_data (keyfile, data, data_len, G_KEY_FILE_KEEP_TRANSLATIONS, error))
    goto out;

  if (g_str_has_suffix (name, ".service"))
    {
      g_autofree gchar *dbus_name = NULL;
      g_autofree gchar *expected_dbus_name = g_strndup (name, strlen (name) - strlen (".service"));

      dbus_name = g_key_file_get_string (keyfile, "D-BUS Service", "Name", NULL);

      if (dbus_name == NULL || strcmp (dbus_name, expected_dbus_name) != 0)
        {
          return flatpak_fail_error (error, FLATPAK_ERROR_EXPORT_FAILED,
                                     _("D-Bus service file '%s' has wrong name"), name);
        }
    }

  if (g_str_has_suffix (name, ".desktop"))
    {
      gsize length;
      g_auto(GStrv) tags = g_key_file_get_string_list (metadata,
                                                       "Application",
                                                       "tags", &length,
                                                       NULL);

      if (tags != NULL)
        {
          g_key_file_set_string_list (keyfile,
                                      G_KEY_FILE_DESKTOP_GROUP,
                                      "X-Flatpak-Tags",
                                      (const char * const *) tags, length);
        }

      /* Add a marker so consumers can easily find out that this launches a sandbox */
      g_key_file_set_string (keyfile, G_KEY_FILE_DESKTOP_GROUP, "X-Flatpak", app);

      /* If the app has been renamed, add its old .desktop filename to
       * X-Flatpak-RenamedFrom in the new .desktop file, taking care not to
       * introduce duplicates.
       */
      if (previous_ids != NULL)
        {
          const char *X_FLATPAK_RENAMED_FROM = "X-Flatpak-RenamedFrom";
          g_auto(GStrv) renamed_from = g_key_file_get_string_list (keyfile,
                                                                   G_KEY_FILE_DESKTOP_GROUP,
                                                                   X_FLATPAK_RENAMED_FROM,
                                                                   NULL, NULL);
          g_autoptr(GPtrArray) merged = g_ptr_array_new_with_free_func (g_free);
          g_autoptr(GHashTable) seen = g_hash_table_new (g_str_hash, g_str_equal);
          const char *new_suffix;

          for (i = 0; renamed_from != NULL && renamed_from[i] != NULL; i++)
            {
              if (!g_hash_table_contains (seen, renamed_from[i]))
                {
                  gchar *copy = g_strdup (renamed_from[i]);
                  g_hash_table_insert (seen, copy, copy);
                  g_ptr_array_add (merged, g_steal_pointer (&copy));
                }
            }

          /* If an app was renamed from com.example.Foo to net.example.Bar, and
           * the new version exports net.example.Bar-suffix.desktop, we assume the
           * old version exported com.example.Foo-suffix.desktop.
           *
           * This assertion is true because
           * flatpak_name_matches_one_wildcard_prefix() is called on all
           * exported files before we get here.
           */
          g_assert (g_str_has_prefix (name, app));
          /* ".desktop" for the "main" desktop file; something like
           * "-suffix.desktop" for extra ones.
           */
          new_suffix = name + strlen (app);

          for (i = 0; previous_ids[i] != NULL; i++)
            {
              g_autofree gchar *previous_desktop = g_strconcat (previous_ids[i], new_suffix, NULL);
              if (!g_hash_table_contains (seen, previous_desktop))
                {
                  g_hash_table_insert (seen, previous_desktop, previous_desktop);
                  g_ptr_array_add (merged, g_steal_pointer (&previous_desktop));
                }
            }

          if (merged->len > 0)
            {
              g_ptr_array_add (merged, NULL);
              g_key_file_set_string_list (keyfile,
                                          G_KEY_FILE_DESKTOP_GROUP,
                                          X_FLATPAK_RENAMED_FROM,
                                          (const char * const *) merged->pdata,
                                          merged->len - 1);
            }
        }
    }

  groups = g_key_file_get_groups (keyfile, NULL);

  for (i = 0; groups[i] != NULL; i++)
    {
      g_auto(GStrv) flatpak_run_opts = g_key_file_get_string_list (keyfile, groups[i], "X-Flatpak-RunOptions", NULL, NULL);
      g_autofree char *flatpak_run_args = format_flatpak_run_args_from_run_opts (flatpak_run_opts);

      g_key_file_remove_key (keyfile, groups[i], "X-Flatpak-RunOptions", NULL);
      g_key_file_remove_key (keyfile, groups[i], "TryExec", NULL);

      /* Remove this to make sure nothing tries to execute it outside the sandbox*/
      g_key_file_remove_key (keyfile, groups[i], "X-GNOME-Bugzilla-ExtraInfoScript", NULL);

      new_exec = g_string_new ("");
      g_string_append_printf (new_exec,
                              FLATPAK_BINDIR "/flatpak run --branch=%s --arch=%s",
                              escaped_branch,
                              escaped_arch);

      if (flatpak_run_args != NULL)
        g_string_append_printf (new_exec, "%s", flatpak_run_args);

      old_exec = g_key_file_get_string (keyfile, groups[i], "Exec", NULL);
      if (old_exec && g_shell_parse_argv (old_exec, &old_argc, &old_argv, NULL) && old_argc >= 1)
        {
          int j;
          g_autofree char *command = maybe_quote (old_argv[0]);

          g_string_append_printf (new_exec, " --command=%s", command);

          for (j = 1; j < old_argc; j++)
            {
              if (strcasecmp (old_argv[j], "%f") == 0 ||
                  strcasecmp (old_argv[j], "%u") == 0)
                {
                  g_string_append (new_exec, " --file-forwarding");
                  break;
                }
            }

          g_string_append (new_exec, " ");
          g_string_append (new_exec, escaped_app);

          for (j = 1; j < old_argc; j++)
            {
              g_autofree char *arg = maybe_quote (old_argv[j]);

              if (strcasecmp (arg, "%f") == 0)
                g_string_append_printf (new_exec, " @@ %s @@", arg);
              else if (strcasecmp (arg, "%u") == 0)
                g_string_append_printf (new_exec, " @@u %s @@", arg);
              else if (g_str_has_prefix (arg, "@@"))
                g_print (_("Skipping invalid Exec argument %s\n"), arg);
              else
                g_string_append_printf (new_exec, " %s", arg);
            }
        }
      else
        {
          g_string_append (new_exec, " ");
          g_string_append (new_exec, escaped_app);
        }

      g_key_file_set_string (keyfile, groups[i], G_KEY_FILE_DESKTOP_KEY_EXEC, new_exec->str);
    }

  new_data = g_key_file_to_data (keyfile, &new_data_len, error);
  if (new_data == NULL)
    goto out;

  if (!flatpak_open_in_tmpdir_at (parent_fd, 0755, tmpfile_name, &out_stream, cancellable, error))
    goto out;

  if (!g_output_stream_write_all (out_stream, new_data, new_data_len, NULL, cancellable, error))
    goto out;

  if (!g_output_stream_close (out_stream, cancellable, error))
    goto out;

  if (target)
    *target = g_steal_pointer (&tmpfile_name);

  ret = TRUE;
out:

  if (new_exec != NULL)
    g_string_free (new_exec, TRUE);

  return ret;
}