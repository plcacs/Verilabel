handle_spawn (PortalFlatpak         *object,
              GDBusMethodInvocation *invocation,
              GUnixFDList           *fd_list,
              const gchar           *arg_cwd_path,
              const gchar *const    *arg_argv,
              GVariant              *arg_fds,
              GVariant              *arg_envs,
              guint                  arg_flags,
              GVariant              *arg_options)
{
  g_autoptr(GError) error = NULL;
  ChildSetupData child_setup_data = { NULL };
  GPid pid;
  PidData *pid_data;
  InstanceIdReadData *instance_id_read_data = NULL;
  gsize i, j, n_fds, n_envs;
  const gint *fds = NULL;
  gint fds_len = 0;
  g_autofree FdMapEntry *fd_map = NULL;
  gchar **env;
  gint32 max_fd;
  GKeyFile *app_info;
  g_autoptr(GPtrArray) flatpak_argv = g_ptr_array_new_with_free_func (g_free);
  g_autofree char *app_id = NULL;
  g_autofree char *branch = NULL;
  g_autofree char *arch = NULL;
  g_autofree char *app_commit = NULL;
  g_autofree char *runtime_ref = NULL;
  g_auto(GStrv) runtime_parts = NULL;
  g_autofree char *runtime_commit = NULL;
  g_autofree char *instance_path = NULL;
  g_auto(GStrv) extra_args = NULL;
  g_auto(GStrv) shares = NULL;
  g_auto(GStrv) sockets = NULL;
  g_auto(GStrv) devices = NULL;
  g_auto(GStrv) unset_env = NULL;
  g_auto(GStrv) sandbox_expose = NULL;
  g_auto(GStrv) sandbox_expose_ro = NULL;
  g_autoptr(GVariant) sandbox_expose_fd = NULL;
  g_autoptr(GVariant) sandbox_expose_fd_ro = NULL;
  g_autoptr(GOutputStream) instance_id_out_stream = NULL;
  guint sandbox_flags = 0;
  gboolean sandboxed;
  gboolean expose_pids;
  gboolean share_pids;
  gboolean notify_start;
  gboolean devel;
  g_autoptr(GString) env_string = g_string_new ("");

  child_setup_data.instance_id_fd = -1;
  child_setup_data.env_fd = -1;

  if (fd_list != NULL)
    fds = g_unix_fd_list_peek_fds (fd_list, &fds_len);

  app_info = g_object_get_data (G_OBJECT (invocation), "app-info");
  g_assert (app_info != NULL);

  app_id = g_key_file_get_string (app_info,
                                  FLATPAK_METADATA_GROUP_APPLICATION,
                                  FLATPAK_METADATA_KEY_NAME, NULL);
  g_assert (app_id != NULL);

  g_debug ("spawn() called from app: '%s'", app_id);
  if (*app_id == 0)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "org.freedesktop.portal.Flatpak.Spawn only works in a flatpak");
      return G_DBUS_METHOD_INVOCATION_HANDLED;
    }

  if (*arg_cwd_path == 0)
    arg_cwd_path = NULL;

  if (arg_argv == NULL || *arg_argv == NULL)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "No command given");
      return G_DBUS_METHOD_INVOCATION_HANDLED;
    }

  if ((arg_flags & ~FLATPAK_SPAWN_FLAGS_ALL) != 0)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Unsupported flags enabled: 0x%x", arg_flags & ~FLATPAK_SPAWN_FLAGS_ALL);
      return G_DBUS_METHOD_INVOCATION_HANDLED;
    }

  runtime_ref = g_key_file_get_string (app_info,
                                       FLATPAK_METADATA_GROUP_APPLICATION,
                                       FLATPAK_METADATA_KEY_RUNTIME, NULL);
  if (runtime_ref == NULL)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "No runtime found");
      return G_DBUS_METHOD_INVOCATION_HANDLED;
    }

  runtime_parts = g_strsplit (runtime_ref, "/", -1);

  branch = g_key_file_get_string (app_info,
                                  FLATPAK_METADATA_GROUP_INSTANCE,
                                  FLATPAK_METADATA_KEY_BRANCH, NULL);
  instance_path = g_key_file_get_string (app_info,
                                         FLATPAK_METADATA_GROUP_INSTANCE,
                                         FLATPAK_METADATA_KEY_INSTANCE_PATH, NULL);
  arch = g_key_file_get_string (app_info,
                                FLATPAK_METADATA_GROUP_INSTANCE,
                                FLATPAK_METADATA_KEY_ARCH, NULL);
  extra_args = g_key_file_get_string_list (app_info,
                                           FLATPAK_METADATA_GROUP_INSTANCE,
                                           FLATPAK_METADATA_KEY_EXTRA_ARGS, NULL, NULL);
  app_commit = g_key_file_get_string (app_info,
                                      FLATPAK_METADATA_GROUP_INSTANCE,
                                      FLATPAK_METADATA_KEY_APP_COMMIT, NULL);
  runtime_commit = g_key_file_get_string (app_info,
                                          FLATPAK_METADATA_GROUP_INSTANCE,
                                          FLATPAK_METADATA_KEY_RUNTIME_COMMIT, NULL);
  shares = g_key_file_get_string_list (app_info, FLATPAK_METADATA_GROUP_CONTEXT,
                                       FLATPAK_METADATA_KEY_SHARED, NULL, NULL);
  sockets = g_key_file_get_string_list (app_info, FLATPAK_METADATA_GROUP_CONTEXT,
                                       FLATPAK_METADATA_KEY_SOCKETS, NULL, NULL);
  devices = g_key_file_get_string_list (app_info, FLATPAK_METADATA_GROUP_CONTEXT,
                                        FLATPAK_METADATA_KEY_DEVICES, NULL, NULL);

  devel = g_key_file_get_boolean (app_info, FLATPAK_METADATA_GROUP_INSTANCE,
                                  FLATPAK_METADATA_KEY_DEVEL, NULL);

  g_variant_lookup (arg_options, "sandbox-expose", "^as", &sandbox_expose);
  g_variant_lookup (arg_options, "sandbox-expose-ro", "^as", &sandbox_expose_ro);
  g_variant_lookup (arg_options, "sandbox-flags", "u", &sandbox_flags);
  sandbox_expose_fd = g_variant_lookup_value (arg_options, "sandbox-expose-fd", G_VARIANT_TYPE ("ah"));
  sandbox_expose_fd_ro = g_variant_lookup_value (arg_options, "sandbox-expose-fd-ro", G_VARIANT_TYPE ("ah"));
  g_variant_lookup (arg_options, "unset-env", "^as", &unset_env);

  if ((sandbox_flags & ~FLATPAK_SPAWN_SANDBOX_FLAGS_ALL) != 0)
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                                             "Unsupported sandbox flags enabled: 0x%x", arg_flags & ~FLATPAK_SPAWN_SANDBOX_FLAGS_ALL);
      return G_DBUS_METHOD_INVOCATION_HANDLED;
    }

  if (instance_path == NULL &&
      ((sandbox_expose != NULL && sandbox_expose[0] != NULL) ||
       (sandbox_expose_ro != NULL && sandbox_expose_ro[0] != NULL)))
    {
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                             G_DBUS_ERROR_INVALID_ARGS,
                                             "Invalid sandbox expose, caller has no instance path");
      return G_DBUS_METHOD_INVOCATION_HANDLED;
    }

  for (i = 0; sandbox_expose != NULL && sandbox_expose[i] != NULL; i++)
    {
      const char *expose = sandbox_expose[i];

      g_debug ("exposing %s", expose);
      if (!is_valid_expose (expose, &error))
        {
          g_dbus_method_invocation_return_gerror (invocation, error);
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }
    }

  for (i = 0; sandbox_expose_ro != NULL && sandbox_expose_ro[i] != NULL; i++)
    {
      const char *expose = sandbox_expose_ro[i];
      g_debug ("exposing %s", expose);
      if (!is_valid_expose (expose, &error))
        {
          g_dbus_method_invocation_return_gerror (invocation, error);
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }
    }

  g_debug ("Running spawn command %s", arg_argv[0]);

  n_fds = 0;
  if (fds != NULL)
    n_fds = g_variant_n_children (arg_fds);
  fd_map = g_new0 (FdMapEntry, n_fds);

  child_setup_data.fd_map = fd_map;
  child_setup_data.fd_map_len = n_fds;

  max_fd = -1;
  for (i = 0; i < n_fds; i++)
    {
      gint32 handle, dest_fd;
      int handle_fd;

      g_variant_get_child (arg_fds, i, "{uh}", &dest_fd, &handle);

      if (handle >= fds_len || handle < 0)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "No file descriptor for handle %d",
                                                 handle);
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      handle_fd = fds[handle];

      fd_map[i].to = dest_fd;
      fd_map[i].from = handle_fd;
      fd_map[i].final = fd_map[i].to;

      /* If stdin/out/err is a tty we try to set it as the controlling
         tty for the app, this way we can use this to run in a terminal. */
      if ((dest_fd == 0 || dest_fd == 1 || dest_fd == 2) &&
          !child_setup_data.set_tty &&
          isatty (handle_fd))
        {
          child_setup_data.set_tty = TRUE;
          child_setup_data.tty = handle_fd;
        }

      max_fd = MAX (max_fd, fd_map[i].to);
      max_fd = MAX (max_fd, fd_map[i].from);
    }

  /* We make a second pass over the fds to find if any "to" fd index
     overlaps an already in use fd (i.e. one in the "from" category
     that are allocated randomly). If a fd overlaps "to" fd then its
     a caller issue and not our fault, so we ignore that. */
  for (i = 0; i < n_fds; i++)
    {
      int to_fd = fd_map[i].to;
      gboolean conflict = FALSE;

      /* At this point we're fine with using "from" values for this
         value (because we handle to==from in the code), or values
         that are before "i" in the fd_map (because those will be
         closed at this point when dup:ing). However, we can't
         reuse a fd that is in "from" for j > i. */
      for (j = i + 1; j < n_fds; j++)
        {
          int from_fd = fd_map[j].from;
          if (from_fd == to_fd)
            {
              conflict = TRUE;
              break;
            }
        }

      if (conflict)
        fd_map[i].to = ++max_fd;
    }

  /* TODO: Ideally we should let `flatpak run` inherit the portal's
   * environment, in case e.g. a LD_LIBRARY_PATH is needed to be able
   * to run `flatpak run`, but tell it to start from a blank environment
   * when running the Flatpak app; but this isn't currently possible, so
   * for now we preserve existing behaviour. */
  if (arg_flags & FLATPAK_SPAWN_FLAGS_CLEAR_ENV)
    {
      char *empty[] = { NULL };
      env = g_strdupv (empty);
    }
  else
    env = g_get_environ ();

  g_ptr_array_add (flatpak_argv, g_strdup ("flatpak"));
  g_ptr_array_add (flatpak_argv, g_strdup ("run"));

  sandboxed = (arg_flags & FLATPAK_SPAWN_FLAGS_SANDBOX) != 0;

  if (sandboxed)
    {
      g_ptr_array_add (flatpak_argv, g_strdup ("--sandbox"));

      if (sandbox_flags & FLATPAK_SPAWN_SANDBOX_FLAGS_SHARE_DISPLAY)
        {
          if (sockets != NULL && g_strv_contains ((const char * const *) sockets, "wayland"))
            g_ptr_array_add (flatpak_argv, g_strdup ("--socket=wayland"));
          if (sockets != NULL && g_strv_contains ((const char * const *) sockets, "fallback-x11"))
            g_ptr_array_add (flatpak_argv, g_strdup ("--socket=fallback-x11"));
          if (sockets != NULL && g_strv_contains ((const char * const *) sockets, "x11"))
            g_ptr_array_add (flatpak_argv, g_strdup ("--socket=x11"));
          if (shares != NULL && g_strv_contains ((const char * const *) shares, "ipc") &&
              sockets != NULL && (g_strv_contains ((const char * const *) sockets, "fallback-x11") ||
                                  g_strv_contains ((const char * const *) sockets, "x11")))
            g_ptr_array_add (flatpak_argv, g_strdup ("--share=ipc"));
        }
      if (sandbox_flags & FLATPAK_SPAWN_SANDBOX_FLAGS_SHARE_SOUND)
        {
          if (sockets != NULL && g_strv_contains ((const char * const *) sockets, "pulseaudio"))
            g_ptr_array_add (flatpak_argv, g_strdup ("--socket=pulseaudio"));
        }
      if (sandbox_flags & FLATPAK_SPAWN_SANDBOX_FLAGS_SHARE_GPU)
        {
          if (devices != NULL &&
              (g_strv_contains ((const char * const *) devices, "dri") ||
               g_strv_contains ((const char * const *) devices, "all")))
            g_ptr_array_add (flatpak_argv, g_strdup ("--device=dri"));
        }
      if (sandbox_flags & FLATPAK_SPAWN_SANDBOX_FLAGS_ALLOW_DBUS)
        g_ptr_array_add (flatpak_argv, g_strdup ("--session-bus"));
      if (sandbox_flags & FLATPAK_SPAWN_SANDBOX_FLAGS_ALLOW_A11Y)
        g_ptr_array_add (flatpak_argv, g_strdup ("--a11y-bus"));
    }
  else
    {
      for (i = 0; extra_args != NULL && extra_args[i] != NULL; i++)
        {
          if (g_str_has_prefix (extra_args[i], "--env="))
            {
              const char *var_val = extra_args[i] + strlen ("--env=");

              if (var_val[0] == '\0' || var_val[0] == '=')
                {
                  g_warning ("Environment variable in extra-args has empty name");
                  continue;
                }

              if (strchr (var_val, '=') == NULL)
                {
                  g_warning ("Environment variable in extra-args has no value");
                  continue;
                }

              g_string_append (env_string, var_val);
              g_string_append_c (env_string, '\0');
            }
          else
            {
              g_ptr_array_add (flatpak_argv, g_strdup (extra_args[i]));
            }
        }
    }

  /* Let the environment variables given by the caller override the ones
   * from extra_args. Don't add them to @env, because they are controlled
   * by our caller, which might be trying to use them to inject code into
   * flatpak(1); add them to the environment block instead.
   *
   * We don't use --env= here, so that if the values are something that
   * should not be exposed to other uids, they can remain confidential. */
  n_envs = g_variant_n_children (arg_envs);
  for (i = 0; i < n_envs; i++)
    {
      const char *var = NULL;
      const char *val = NULL;
      g_variant_get_child (arg_envs, i, "{&s&s}", &var, &val);

      if (var[0] == '\0')
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Environment variable cannot have empty name");
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      if (strchr (var, '=') != NULL)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Environment variable name cannot contain '='");
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      g_string_append (env_string, var);
      g_string_append_c (env_string, '=');
      g_string_append (env_string, val);
      g_string_append_c (env_string, '\0');
    }

  if (env_string->len > 0)
    {
      g_auto(GLnxTmpfile) env_tmpf  = { 0, };

      if (!flatpak_buffer_to_sealed_memfd_or_tmpfile (&env_tmpf, "environ",
                                                      env_string->str,
                                                      env_string->len, &error))
        {
          g_dbus_method_invocation_return_gerror (invocation, error);
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      child_setup_data.env_fd = glnx_steal_fd (&env_tmpf.fd);
      g_ptr_array_add (flatpak_argv,
                       g_strdup_printf ("--env-fd=%d",
                                        child_setup_data.env_fd));
    }

  for (i = 0; unset_env != NULL && unset_env[i] != NULL; i++)
    {
      const char *var = unset_env[i];

      if (var[0] == '\0')
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Environment variable cannot have empty name");
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      if (strchr (var, '=') != NULL)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Environment variable name cannot contain '='");
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      g_ptr_array_add (flatpak_argv,
                       g_strdup_printf ("--unset-env=%s", var));
    }

  expose_pids = (arg_flags & FLATPAK_SPAWN_FLAGS_EXPOSE_PIDS) != 0;
  share_pids = (arg_flags & FLATPAK_SPAWN_FLAGS_SHARE_PIDS) != 0;

  if (expose_pids || share_pids)
    {
      g_autofree char *instance_id = NULL;
      int sender_pid1 = 0;

      if (!(supports & FLATPAK_SPAWN_SUPPORT_FLAGS_EXPOSE_PIDS))
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_NOT_SUPPORTED,
                                                 "Expose pids not supported with setuid bwrap");
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      instance_id = g_key_file_get_string (app_info,
                                           FLATPAK_METADATA_GROUP_INSTANCE,
                                           FLATPAK_METADATA_KEY_INSTANCE_ID, NULL);

      if (instance_id)
        {
          g_autoptr(FlatpakInstance) instance = flatpak_instance_new_for_id (instance_id);
          sender_pid1 = flatpak_instance_get_child_pid (instance);
        }

      if (sender_pid1 == 0)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                 G_DBUS_ERROR_INVALID_ARGS,
                                                 "Could not find requesting pid");
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      g_ptr_array_add (flatpak_argv, g_strdup_printf ("--parent-pid=%d", sender_pid1));

      if (share_pids)
        g_ptr_array_add (flatpak_argv, g_strdup ("--parent-share-pids"));
      else
        g_ptr_array_add (flatpak_argv, g_strdup ("--parent-expose-pids"));
    }

  notify_start = (arg_flags & FLATPAK_SPAWN_FLAGS_NOTIFY_START) != 0;
  if (notify_start)
    {
      int pipe_fds[2];
      if (pipe (pipe_fds) == -1)
        {
          int errsv = errno;
          g_dbus_method_invocation_return_error (invocation, G_IO_ERROR,
                                                 g_io_error_from_errno (errsv),
                                                 "Failed to create instance ID pipe: %s",
                                                 g_strerror (errsv));
          return G_DBUS_METHOD_INVOCATION_HANDLED;
        }

      GInputStream *in_stream = G_INPUT_STREAM (g_unix_input_stream_new (pipe_fds[0], TRUE));
      /* This is saved to ensure the portal's end gets closed after the exec. */
      instance_id_out_stream = G_OUTPUT_STREAM (g_unix_output_stream_new (pipe_fds[1], TRUE));

      instance_id_read_data = g_new0 (InstanceIdReadData, 1);

      g_input_stream_read_async (in_stream, instance_id_read_data->buffer,
                                 INSTANCE_ID_BUFFER_SIZE - 1, G_PRIORITY_DEFAULT, NULL,
                                 instance_id_read_finish, instance_id_read_data);

      g_ptr_array_add (flatpak_argv, g_strdup_printf ("--instance-id-fd=%d", pipe_fds[1]));
      child_setup_data.instance_id_fd = pipe_fds[1];
    }

  if (devel)
    g_ptr_array_add (flatpak_argv, g_strdup ("--devel"));

  /* Inherit launcher network access from launcher, unless
     NO_NETWORK set. */
  if (shares != NULL && g_strv_contains ((const char * const *) shares, "network") &&
      !(arg_flags & FLATPAK_SPAWN_FLAGS_NO_NETWORK))
    g_ptr_array_add (flatpak_argv, g_strdup ("--share=network"));
  else
    g_ptr_array_add (flatpak_argv, g_strdup ("--unshare=network"));


  if (instance_path)
    {
      for (i = 0; sandbox_expose != NULL && sandbox_expose[i] != NULL; i++)
        g_ptr_array_add (flatpak_argv,
                         filesystem_sandbox_arg (instance_path, sandbox_expose[i], FALSE));
      for (i = 0; sandbox_expose_ro != NULL && sandbox_expose_ro[i] != NULL; i++)
        g_ptr_array_add (flatpak_argv,
                         filesystem_sandbox_arg (instance_path, sandbox_expose_ro[i], TRUE));
    }

  for (i = 0; sandbox_expose_ro != NULL && sandbox_expose_ro[i] != NULL; i++)
    {
      const char *expose = sandbox_expose_ro[i];
      g_debug ("exposing %s", expose);
    }

  if (sandbox_expose_fd != NULL)
    {
      gsize len = g_variant_n_children (sandbox_expose_fd);
      for (i = 0; i < len; i++)
        {
          gint32 handle;
          g_variant_get_child (sandbox_expose_fd, i, "h", &handle);
          if (handle >= 0 && handle < fds_len)
            {
              int handle_fd = fds[handle];
              g_autofree char *path = NULL;
              gboolean writable = FALSE;

              path = get_path_for_fd (handle_fd, &writable, &error);

              if (path)
                {
                  g_ptr_array_add (flatpak_argv, filesystem_arg (path, !writable));
                }
              else
                {
                  g_debug ("unable to get path for sandbox-exposed fd %d, ignoring: %s",
                           handle_fd, error->message);
                  g_clear_error (&error);
                }
            }
          else
            {
              g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                     G_DBUS_ERROR_INVALID_ARGS,
                                                     "No file descriptor for handle %d",
                                                     handle);
              return G_DBUS_METHOD_INVOCATION_HANDLED;
            }
        }
    }

  if (sandbox_expose_fd_ro != NULL)
    {
      gsize len = g_variant_n_children (sandbox_expose_fd_ro);
      for (i = 0; i < len; i++)
        {
          gint32 handle;
          g_variant_get_child (sandbox_expose_fd_ro, i, "h", &handle);
          if (handle >= 0 && handle < fds_len)
            {
              int handle_fd = fds[handle];
              g_autofree char *path = NULL;
              gboolean writable = FALSE;

              path = get_path_for_fd (handle_fd, &writable, &error);

              if (path)
                {
                  g_ptr_array_add (flatpak_argv, filesystem_arg (path, TRUE));
                }
              else
                {
                  g_debug ("unable to get path for sandbox-exposed fd %d, ignoring: %s",
                           handle_fd, error->message);
                  g_clear_error (&error);
                }
            }
          else
            {
              g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR,
                                                     G_DBUS_ERROR_INVALID_ARGS,
                                                     "No file descriptor for handle %d",
                                                     handle);
              return G_DBUS_METHOD_INVOCATION_HANDLED;
            }
        }
    }

  g_ptr_array_add (flatpak_argv, g_strdup_printf ("--runtime=%s", runtime_parts[1]));
  g_ptr_array_add (flatpak_argv, g_strdup_printf ("--runtime-version=%s", runtime_parts[3]));

  if ((arg_flags & FLATPAK_SPAWN_FLAGS_LATEST_VERSION) == 0)
    {
      if (app_commit)
        g_ptr_array_add (flatpak_argv, g_strdup_printf ("--commit=%s", app_commit));
      if (runtime_commit)
        g_ptr_array_add (flatpak_argv, g_strdup_printf ("--runtime-commit=%s", runtime_commit));
    }

  if (arg_cwd_path != NULL)
    g_ptr_array_add (flatpak_argv, g_strdup_printf ("--cwd=%s", arg_cwd_path));

  if (arg_argv[0][0] != 0)
    g_ptr_array_add (flatpak_argv, g_strdup_printf ("--command=%s", arg_argv[0]));

  g_ptr_array_add (flatpak_argv, g_strdup_printf ("%s/%s/%s", app_id, arch ? arch : "", branch ? branch : ""));
  for (i = 1; arg_argv[i] != NULL; i++)
    g_ptr_array_add (flatpak_argv, g_strdup (arg_argv[i]));
  g_ptr_array_add (flatpak_argv, NULL);

  if (opt_verbose)
    {
      g_autoptr(GString) cmd = g_string_new ("");

      for (i = 0; flatpak_argv->pdata[i] != NULL; i++)
        {
          if (i > 0)
            g_string_append (cmd, " ");
          g_string_append (cmd, flatpak_argv->pdata[i]);
        }

      g_debug ("Starting: %s\n", cmd->str);
    }

  /* We use LEAVE_DESCRIPTORS_OPEN to work around dead-lock, see flatpak_close_fds_workaround */
  if (!g_spawn_async_with_pipes (NULL,
                                 (char **) flatpak_argv->pdata,
                                 env,
                                 G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
                                 child_setup_func, &child_setup_data,
                                 &pid,
                                 NULL,
                                 NULL,
                                 NULL,
                                 &error))
    {
      gint code = G_DBUS_ERROR_FAILED;
      if (g_error_matches (error, G_SPAWN_ERROR, G_SPAWN_ERROR_ACCES))
        code = G_DBUS_ERROR_ACCESS_DENIED;
      else if (g_error_matches (error, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT))
        code = G_DBUS_ERROR_FILE_NOT_FOUND;
      g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, code,
                                             "Failed to start command: %s",
                                             error->message);
      return G_DBUS_METHOD_INVOCATION_HANDLED;
    }

  if (instance_id_read_data)
    instance_id_read_data->pid = pid;

  pid_data = g_new0 (PidData, 1);
  pid_data->pid = pid;
  pid_data->client = g_strdup (g_dbus_method_invocation_get_sender (invocation));
  pid_data->watch_bus = (arg_flags & FLATPAK_SPAWN_FLAGS_WATCH_BUS) != 0;
  pid_data->expose_or_share_pids = (expose_pids || share_pids);
  pid_data->child_watch = g_child_watch_add_full (G_PRIORITY_DEFAULT,
                                                  pid,
                                                  child_watch_died,
                                                  pid_data,
                                                  NULL);

  g_debug ("Client Pid is %d", pid_data->pid);

  g_hash_table_replace (client_pid_data_hash, GUINT_TO_POINTER (pid_data->pid),
                        pid_data);

  portal_flatpak_complete_spawn (object, invocation, NULL, pid);
  return G_DBUS_METHOD_INVOCATION_HANDLED;
}