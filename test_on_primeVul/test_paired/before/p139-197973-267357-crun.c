crun_command_exec (struct crun_global_arguments *global_args, int argc, char **argv, libcrun_error_t *err)
{
  int first_arg = 0, ret = 0;
  libcrun_context_t crun_context = {
    0,
  };
  cleanup_process_schema runtime_spec_schema_config_schema_process *process = NULL;
  struct libcrun_container_exec_options_s exec_opts;

  memset (&exec_opts, 0, sizeof (exec_opts));
  exec_opts.struct_size = sizeof (exec_opts);

  crun_context.preserve_fds = 0;
  crun_context.listen_fds = 0;

  argp_parse (&run_argp, argc, argv, ARGP_IN_ORDER, &first_arg, &exec_options);
  crun_assert_n_args (argc - first_arg, exec_options.process ? 1 : 2, -1);

  ret = init_libcrun_context (&crun_context, argv[first_arg], global_args, err);
  if (UNLIKELY (ret < 0))
    return ret;

  crun_context.detach = exec_options.detach;
  crun_context.console_socket = exec_options.console_socket;
  crun_context.pid_file = exec_options.pid_file;
  crun_context.preserve_fds = exec_options.preserve_fds;

  if (getenv ("LISTEN_FDS"))
    {
      crun_context.listen_fds = strtoll (getenv ("LISTEN_FDS"), NULL, 10);
      crun_context.preserve_fds += crun_context.listen_fds;
    }

  if (exec_options.process)
    exec_opts.path = exec_options.process;
  else
    {
      process = xmalloc0 (sizeof (*process));
      int i;

      process->args_len = argc;
      process->args = xmalloc0 ((argc + 1) * sizeof (*process->args));
      for (i = 0; i < argc - first_arg; i++)
        process->args[i] = xstrdup (argv[first_arg + i + 1]);
      process->args[i] = NULL;
      if (exec_options.cwd)
        process->cwd = exec_options.cwd;
      process->terminal = exec_options.tty;
      process->env = exec_options.env;
      process->env_len = exec_options.env_size;
      process->user = make_oci_process_user (exec_options.user);

      if (exec_options.process_label != NULL)
        process->selinux_label = exec_options.process_label;

      if (exec_options.apparmor != NULL)
        process->apparmor_profile = exec_options.apparmor;

      if (exec_options.cap_size > 0)
        {
          runtime_spec_schema_config_schema_process_capabilities *capabilities
              = xmalloc (sizeof (runtime_spec_schema_config_schema_process_capabilities));

          capabilities->effective = exec_options.cap;
          capabilities->effective_len = exec_options.cap_size;

          capabilities->inheritable = dup_array (exec_options.cap, exec_options.cap_size);
          capabilities->inheritable_len = exec_options.cap_size;

          capabilities->bounding = dup_array (exec_options.cap, exec_options.cap_size);
          capabilities->bounding_len = exec_options.cap_size;

          capabilities->ambient = dup_array (exec_options.cap, exec_options.cap_size);
          capabilities->ambient_len = exec_options.cap_size;

          capabilities->permitted = dup_array (exec_options.cap, exec_options.cap_size);
          capabilities->permitted_len = exec_options.cap_size;

          process->capabilities = capabilities;
        }

      // noNewPriviledges will remain `false` if basespec has `false` unless specified
      // Default is always `true` in generated basespec config
      if (exec_options.no_new_privs)
        process->no_new_privileges = 1;

      exec_opts.process = process;
    }

  exec_opts.cgroup = exec_options.cgroup;

  return libcrun_container_exec_with_options (&crun_context, argv[first_arg], &exec_opts, err);
}