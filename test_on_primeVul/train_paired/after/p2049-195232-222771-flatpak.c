flatpak_context_merge (FlatpakContext *context,
                       FlatpakContext *other)
{
  GHashTableIter iter;
  gpointer key, value;
  gboolean no_home = FALSE;
  gboolean no_host = FALSE;

  context->shares &= ~other->shares_valid;
  context->shares |= other->shares;
  context->shares_valid |= other->shares_valid;
  context->sockets &= ~other->sockets_valid;
  context->sockets |= other->sockets;
  context->sockets_valid |= other->sockets_valid;
  context->devices &= ~other->devices_valid;
  context->devices |= other->devices;
  context->devices_valid |= other->devices_valid;
  context->features &= ~other->features_valid;
  context->features |= other->features;
  context->features_valid |= other->features_valid;

  g_hash_table_iter_init (&iter, other->env_vars);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (context->env_vars, g_strdup (key), g_strdup (value));

  g_hash_table_iter_init (&iter, other->persistent);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (context->persistent, g_strdup (key), value);

  /* We first handle all negative home and host as they override other
     keys than themselves from the parent */
  if (g_hash_table_lookup_extended (other->filesystems,
                                    "host",
                                    NULL, &value))
    {
      FlatpakFilesystemMode host_mode = GPOINTER_TO_INT (value);
      if (host_mode == FLATPAK_FILESYSTEM_MODE_NONE)
        no_host = TRUE;
    }

  if (g_hash_table_lookup_extended (other->filesystems,
                                    "home",
                                    NULL, &value))
    {
      FlatpakFilesystemMode home_mode = GPOINTER_TO_INT (value);
      if (home_mode == FLATPAK_FILESYSTEM_MODE_NONE)
        no_home = TRUE;
    }

  if (no_host)
    {
      g_hash_table_remove_all (context->filesystems);
    }
  else if (no_home)
    {
      g_hash_table_iter_init (&iter, context->filesystems);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (flatpak_filesystem_key_in_home ((const char *)key))
            g_hash_table_iter_remove (&iter);
        }
    }

  /* Then set the new ones, which includes propagating the nohost and nohome ones. */
  g_hash_table_iter_init (&iter, other->filesystems);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (context->filesystems, g_strdup (key), value);

  g_hash_table_iter_init (&iter, other->session_bus_policy);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (context->session_bus_policy, g_strdup (key), value);

  g_hash_table_iter_init (&iter, other->system_bus_policy);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (context->system_bus_policy, g_strdup (key), value);

  g_hash_table_iter_init (&iter, other->system_bus_policy);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (context->system_bus_policy, g_strdup (key), value);

  g_hash_table_iter_init (&iter, other->generic_policy);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char **policy_values = (const char **) value;
      int i;

      for (i = 0; policy_values[i] != NULL; i++)
        flatpak_context_apply_generic_policy (context, (char *) key, policy_values[i]);
    }
}