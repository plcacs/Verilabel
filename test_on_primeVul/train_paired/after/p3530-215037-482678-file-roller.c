_g_file_remove_directory (GFile         *directory,
			  GCancellable  *cancellable,
			  GError       **error)
{
	GFileEnumerator *enumerator;
	GFileInfo       *info;
	gboolean         error_occurred = FALSE;

	if (directory == NULL)
		return TRUE;

	enumerator = g_file_enumerate_children (directory,
					        G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					        cancellable,
					        error);

	while (! error_occurred && (info = g_file_enumerator_next_file (enumerator, cancellable, error)) != NULL) {
		GFile *child;

		child = g_file_get_child (directory, g_file_info_get_name (info));
		switch (g_file_info_get_file_type (info)) {
		case G_FILE_TYPE_DIRECTORY:
			if (! _g_file_remove_directory (child, cancellable, error))
				error_occurred = TRUE;
			break;
		default:
			if (! g_file_delete (child, cancellable, error))
				error_occurred = TRUE;
			break;
		}

		g_object_unref (child);
		g_object_unref (info);
	}

	if (! error_occurred && ! g_file_delete (directory, cancellable, error))
 		error_occurred = TRUE;

	g_object_unref (enumerator);

	return ! error_occurred;
}