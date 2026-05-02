extract_archive_thread (GSimpleAsyncResult *result,
			GObject            *object,
			GCancellable       *cancellable)
{
	ExtractData          *extract_data;
	LoadData             *load_data;
	GHashTable           *checked_folders;
	struct archive       *a;
	struct archive_entry *entry;
	int                   r;

	extract_data = g_simple_async_result_get_op_res_gpointer (result);
	load_data = LOAD_DATA (extract_data);

	checked_folders = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);
	fr_archive_progress_set_total_files (load_data->archive, extract_data->n_files_to_extract);

	a = archive_read_new ();
	archive_read_support_filter_all (a);
	archive_read_support_format_all (a);
	archive_read_open (a, load_data, load_data_open, load_data_read, load_data_close);
	while ((r = archive_read_next_header (a, &entry)) == ARCHIVE_OK) {
		const char    *pathname;
		char          *fullpath;
		GFile         *file;
		GFile         *parent;
		GOutputStream *ostream;
		const void    *buffer;
		size_t         buffer_size;
		int64_t        offset;
		GError        *local_error = NULL;
		__LA_MODE_T    filetype;

		if (g_cancellable_is_cancelled (cancellable))
			break;

		pathname = archive_entry_pathname (entry);
		if (! extract_data_get_extraction_requested (extract_data, pathname)) {
			archive_read_data_skip (a);
			continue;
		}

		fullpath = (*pathname == '/') ? g_strdup (pathname) : g_strconcat ("/", pathname, NULL);
		file = g_file_get_child (extract_data->destination, _g_path_get_relative_basename (fullpath, extract_data->base_dir, extract_data->junk_paths));

		/* honor the skip_older and overwrite options */

		if (extract_data->skip_older || ! extract_data->overwrite) {
			GFileInfo *info;

			info = g_file_query_info (file,
						  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," G_FILE_ATTRIBUTE_TIME_MODIFIED,
						  G_FILE_QUERY_INFO_NONE,
						  cancellable,
						  &local_error);
			if (info != NULL) {
				gboolean skip = FALSE;

				if (! extract_data->overwrite) {
					skip = TRUE;
				}
				else if (extract_data->skip_older) {
					GTimeVal modification_time;

					g_file_info_get_modification_time (info, &modification_time);
					if (archive_entry_mtime (entry) < modification_time.tv_sec)
						skip = TRUE;
				}

				g_object_unref (info);

				if (skip) {
					g_object_unref (file);

					archive_read_data_skip (a);
					fr_archive_progress_inc_completed_bytes (load_data->archive, archive_entry_size_is_set (entry) ? archive_entry_size (entry) : 0);

					if ((extract_data->file_list != NULL) && (--extract_data->n_files_to_extract == 0)) {
						r = ARCHIVE_EOF;
						break;
					}

					continue;
				}
			}
			else {
				if (! g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
					load_data->error = local_error;
					g_object_unref (info);
					break;
				}
				g_error_free (local_error);
			}
		}

		fr_archive_progress_inc_completed_files (load_data->archive, 1);

		/* create the file parents */

		parent = g_file_get_parent (file);

		if ((parent != NULL)
		    && (g_hash_table_lookup (checked_folders, parent) == NULL)
		    && ! g_file_query_exists (parent, cancellable))
		{
			if (g_file_make_directory_with_parents (parent, cancellable, &load_data->error)) {
				GFile *grandparent;

				grandparent = g_object_ref (parent);
				while (grandparent != NULL) {
					if (g_hash_table_lookup (checked_folders, grandparent) == NULL)
						g_hash_table_insert (checked_folders, grandparent, GINT_TO_POINTER (1));
					grandparent = g_file_get_parent (grandparent);
				}
			}
		}
		g_object_unref (parent);

		/* create the file */

		filetype = archive_entry_filetype (entry);

		if (load_data->error == NULL) {
			const char  *linkname;

			linkname = archive_entry_hardlink (entry);
			if (linkname != NULL) {
				char  *link_fullpath;
				GFile *link_file;
				char  *oldname;
				char  *newname;
				int    r;

				link_fullpath = (*linkname == '/') ? g_strdup (linkname) : g_strconcat ("/", linkname, NULL);
				link_file = g_file_get_child (extract_data->destination, _g_path_get_relative_basename (link_fullpath, extract_data->base_dir, extract_data->junk_paths));
				oldname = g_file_get_path (link_file);
				newname = g_file_get_path (file);

				if ((oldname != NULL) && (newname != NULL))
					r = link (oldname, newname);
				else
					r = -1;

				if (r == 0) {
					__LA_INT64_T filesize;

					if (archive_entry_size_is_set (entry))
						filesize = archive_entry_size (entry);
					else
						filesize = -1;

					if (filesize > 0)
						filetype = AE_IFREG; /* treat as a regular file to save the data */
				}
				else {
					char *uri;
					char *msg;

					uri = g_file_get_uri (file);
					msg = g_strdup_printf ("Could not create the hard link %s", uri);
					load_data->error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED, msg);

					g_free (msg);
					g_free (uri);
				}

				g_free (newname);
				g_free (oldname);
				g_object_unref (link_file);
				g_free (link_fullpath);
			}
		}

		if (load_data->error == NULL) {
			switch (filetype) {
			case AE_IFDIR:
				if (! g_file_make_directory (file, cancellable, &local_error)) {
					if (! g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
						load_data->error = g_error_copy (local_error);
					g_error_free (local_error);
				}
				else
					_g_file_set_attributes_from_entry (file, entry, extract_data, cancellable);
				archive_read_data_skip (a);
				break;

			case AE_IFREG:
				ostream = (GOutputStream *) g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, cancellable, &load_data->error);
				if (ostream == NULL)
					break;

				while ((r = archive_read_data_block (a, &buffer, &buffer_size, &offset)) == ARCHIVE_OK) {
					if (g_output_stream_write (ostream, buffer, buffer_size, cancellable, &load_data->error) == -1)
						break;
					fr_archive_progress_inc_completed_bytes (load_data->archive, buffer_size);
				}
				_g_object_unref (ostream);

				if (r != ARCHIVE_EOF)
					load_data->error = g_error_new_literal (FR_ERROR, FR_ERROR_COMMAND_ERROR, archive_error_string (a));
				else
					_g_file_set_attributes_from_entry (file, entry, extract_data, cancellable);
				break;

			case AE_IFLNK:
				if (! g_file_make_symbolic_link (file, archive_entry_symlink (entry), cancellable, &local_error)) {
					if (! g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
						load_data->error = g_error_copy (local_error);
					g_error_free (local_error);
				}
				archive_read_data_skip (a);
				break;

			default:
				archive_read_data_skip (a);
				break;
			}
		}

		g_object_unref (file);
		g_free (fullpath);

		if (load_data->error != NULL)
			break;

		if ((extract_data->file_list != NULL) && (--extract_data->n_files_to_extract == 0)) {
			r = ARCHIVE_EOF;
			break;
		}
	}

	if ((load_data->error == NULL) && (r != ARCHIVE_EOF))
		load_data->error = g_error_new_literal (FR_ERROR, FR_ERROR_COMMAND_ERROR, archive_error_string (a));
	if (load_data->error == NULL)
		g_cancellable_set_error_if_cancelled (cancellable, &load_data->error);
	if (load_data->error != NULL)
		g_simple_async_result_set_from_error (result, load_data->error);

	g_hash_table_unref (checked_folders);
	archive_read_free (a);
	extract_data_free (extract_data);
}