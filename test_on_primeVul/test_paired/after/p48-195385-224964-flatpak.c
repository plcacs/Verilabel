flatpak_dir_ensure_bundle_remote (FlatpakDir         *self,
                                  GFile              *file,
                                  GBytes             *extra_gpg_data,
                                  FlatpakDecomposed **out_ref,
                                  char              **out_checksum,
                                  char              **out_metadata,
                                  gboolean           *out_created_remote,
                                  GCancellable       *cancellable,
                                  GError            **error)
{
  g_autoptr(FlatpakDecomposed) ref = NULL;
  gboolean created_remote = FALSE;
  g_autoptr(GBytes) deploy_data = NULL;
  g_autoptr(GVariant) metadata = NULL;
  g_autofree char *origin = NULL;
  g_autofree char *fp_metadata = NULL;
  g_autofree char *basename = NULL;
  g_autoptr(GBytes) included_gpg_data = NULL;
  GBytes *gpg_data = NULL;
  g_autofree char *to_checksum = NULL;
  g_autofree char *remote = NULL;
  g_autofree char *collection_id = NULL;

  if (!flatpak_dir_ensure_repo (self, cancellable, error))
    return NULL;

  metadata = flatpak_bundle_load (file, &to_checksum,
                                  &ref,
                                  &origin,
                                  NULL, &fp_metadata, NULL,
                                  &included_gpg_data,
                                  &collection_id,
                                  error);
  if (metadata == NULL)
    return NULL;

  /* If we rely on metadata (to e.g. print permissions), check it exists before creating the remote */
  if (out_metadata && fp_metadata == NULL)
    {
      flatpak_fail_error (error, FLATPAK_ERROR_INVALID_DATA, "No metadata in bundler header");
      return NULL;
    }

  gpg_data = extra_gpg_data ? extra_gpg_data : included_gpg_data;

  deploy_data = flatpak_dir_get_deploy_data (self, ref, FLATPAK_DEPLOY_VERSION_ANY, cancellable, NULL);
  if (deploy_data != NULL)
    {
      remote = g_strdup (flatpak_deploy_data_get_origin (deploy_data));

      /* We need to import any gpg keys because otherwise the pull will fail */
      if (gpg_data != NULL)
        {
          g_autoptr(GKeyFile) new_config = NULL;

          new_config = ostree_repo_copy_config (flatpak_dir_get_repo (self));

          if (!flatpak_dir_modify_remote (self, remote, new_config,
                                          gpg_data, cancellable, error))
            return NULL;
        }
    }
  else
    {
      g_autofree char *id = flatpak_decomposed_dup_id (ref);
      /* Add a remote for later updates */
      basename = g_file_get_basename (file);
      remote = flatpak_dir_create_origin_remote (self,
                                                 origin,
                                                 id,
                                                 basename,
                                                 flatpak_decomposed_get_ref (ref),
                                                 gpg_data,
                                                 collection_id,
                                                 &created_remote,
                                                 cancellable,
                                                 error);
      if (remote == NULL)
        return NULL;
    }

  if (out_created_remote)
    *out_created_remote = created_remote;

  if (out_ref)
    *out_ref = g_steal_pointer (&ref);

  if (out_checksum)
    *out_checksum = g_steal_pointer (&to_checksum);

  if (out_metadata)
    *out_metadata = g_steal_pointer (&fp_metadata);


  return g_steal_pointer (&remote);
}