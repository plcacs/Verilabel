plugin_init (Ekiga::KickStart& kickstart)
{
#ifdef DEBUG
  // should make it easier to test ekiga without installing
  gchar* path = g_build_path (G_DIR_SEPARATOR_S,
			      g_get_tmp_dir (), "ekiga_debug_plugins", NULL);
  plugin_parse_directory (kickstart, path);
  g_free (path);
#else
  plugin_parse_directory (kickstart,
			  EKIGA_PLUGIN_DIR);
#endif
}