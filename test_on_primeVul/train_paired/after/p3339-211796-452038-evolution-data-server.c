e_util_can_use_collection_as_credential_source (ESource *collection_source,
						ESource *child_source)
{
	gboolean can_use_collection = FALSE;

	if (collection_source)
		g_return_val_if_fail (E_IS_SOURCE (collection_source), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (child_source), FALSE);

	if (collection_source && e_source_has_extension (collection_source, E_SOURCE_EXTENSION_COLLECTION)) {
		/* Use the found parent collection source for credentials store only if
		   the child source doesn't have any authentication information, or this
		   information is not filled, or if either the host name or the user name
		   are the same with the collection source.

		   This allows to create a collection of sources which has one source
		   (like message send) on a different server, thus this source uses
		   its own credentials.
		*/
		if (!e_source_has_extension (child_source, E_SOURCE_EXTENSION_AUTHENTICATION)) {
			can_use_collection = TRUE;
		} else if (e_source_has_extension (collection_source, E_SOURCE_EXTENSION_AUTHENTICATION)) {
			ESourceAuthentication *auth_source, *auth_collection;
			gchar *host_source, *host_collection;

			auth_source = e_source_get_extension (child_source, E_SOURCE_EXTENSION_AUTHENTICATION);
			auth_collection = e_source_get_extension (collection_source, E_SOURCE_EXTENSION_AUTHENTICATION);

			host_source = e_source_authentication_dup_host (auth_source);
			host_collection = e_source_authentication_dup_host (auth_collection);

			if (host_source && host_collection && g_ascii_strcasecmp (host_source, host_collection) == 0) {
				can_use_collection = TRUE;
			} else {
				/* Only one of them is filled, then use the collection; otherwise
				   both are filled and they do not match, thus do not use collection. */
				can_use_collection = (host_collection && *host_collection && (!host_source || !*host_source)) ||
						     (host_source && *host_source && (!host_collection || !*host_collection));
			}

			g_free (host_source);
			g_free (host_collection);

			if (can_use_collection) {
				gchar *username_source, *username_collection;

				username_source = e_source_authentication_dup_user (auth_source);
				username_collection = e_source_authentication_dup_user (auth_collection);

				/* Check user name similarly as host name */
				if (username_source && username_collection && g_ascii_strcasecmp (username_source, username_collection) == 0) {
					can_use_collection = TRUE;
				} else {
					can_use_collection = !username_source || !*username_source;
				}

				g_free (username_source);
				g_free (username_collection);
			}

			if (can_use_collection) {
				gchar *method_source, *method_collection;

				/* Also check the method; if different, then rather not use the collection.
				   Consider 'none' method on the child as the same as the collection method. */
				method_source = e_source_authentication_dup_method (auth_source);
				method_collection = e_source_authentication_dup_method (auth_collection);

				can_use_collection = !method_source || !method_collection ||
					g_ascii_strcasecmp (method_source, "none") == 0 ||
					g_ascii_strcasecmp (method_source, method_collection) == 0;

				g_free (method_source);
				g_free (method_collection);
			}
		}
	}

	return can_use_collection;
}