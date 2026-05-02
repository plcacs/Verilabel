extract_archive_thread (GSimpleAsyncResult *result,
			GObject            *object,
			GCancellable       *cancellable)
{
	ExtractData          *extract_data;
	LoadData             *load_data;
	GHashTable           *checked_folders;
	GHashTable           *created_files;
	GHashTable           *folders_created_during_extraction;
	GHashTable           *external_links;
	struct archive       *a;
	struct archive_entry *entry;
	int                   r;

	extract_data = g_simple_async_result_get_op_res_gpointer (result);
	load_data = LOAD_DATA (extract_data);

	r = create_read_object (load_data, &a);
	if (r != ARCHIVE_OK) {
		archive_read_free(a);
		return;
	}

	checked_folders = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);
	created_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, g_object_unref);
	folders_created_during_extraction = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);
	external_links = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, NULL);
	fr_archive_progress_set_total_files (load_data->archive, extract_data->n_files_to_extract);

	while ((r = archive_read_next_header (a, &entry)) == ARCHIVE_OK) {
		const char    *pathname;
		char          *fullpath;
		const char    *relative_path;
		GFile         *file;
		GFile         *parent;
		GOutputStream *ostream;
		const void    *buffer;
		size_t         buffer_size;
		int64_t        target_offset, actual_offset;
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
		relative_path = _g_path_get_relative_basename_safe (fullpath, extract_data->base_dir, extract_data->junk_paths);
		if (relative_path == NULL) {
			fr_archive_progress_inc_completed_files (load_data->archive, 1);
			fr_archive_progress_inc_completed_bytes (load_data->archive, archive_entry_size_is_set (entry) ? archive_entry_size (entry) : 0);
			archive_read_data_skip (a);
			continue;
		}

		if (_g_path_is_external_to_destination (relative_path, extract_data->destination, external_links)) {
			fr_archive_progress_inc_completed_files (load_data->archive, 1);
			fr_archive_progress_inc_completed_bytes (load_data->archive, archive_entry_size_is_set (entry) ? archive_entry_size (entry) : 0);
			archive_read_data_skip (a);
			continue;
		}

		file = g_file_get_child (extract_data->destination, relative_path);

		/* honor the skip_older and overwrite options */

		if ((g_hash_table_lookup (folders_created_during_extraction, file) == NULL)
		    && (extract_data->skip_older || ! extract_data->overwrite))
		{
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
				g_clear_error (&local_error);
			}
		}

		fr_archive_progress_inc_completed_files (load_data->archive, 1);

		/* create the file parents */

		parent = g_file_get_parent (file);

		if ((parent != NULL)
		    && (g_hash_table_lookup (checked_folders, parent) == NULL)
		    && ! g_file_query_exists (parent, cancellable))
		{
			if (! _g_file_make_directory_with_parents (parent,
								   folders_created_during_extraction,
								   cancellable,
								   &local_error))
			{
				if (! g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
					load_data->error = local_error;
				else
					g_clear_error (&local_error);
			}

			if (load_data->error == NULL) {
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
				char        *link_fullpath;
				const char  *relative_path;
				GFile       *link_file;
				char        *oldname;
				char        *newname;
				int          r;

				link_fullpath = (*linkname == '/') ? g_strdup (linkname) : g_strconcat ("/", linkname, NULL);
				relative_path = _g_path_get_relative_basename_safe (link_fullpath, extract_data->base_dir, extract_data->junk_paths);
				if (relative_path == NULL) {
					g_free (link_fullpath);
					archive_read_data_skip (a);
					continue;
				}

				link_file = g_file_get_child (extract_data->destination, relative_path);
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
					g_clear_error (&local_error);
				}
				if (load_data->error == NULL)
					g_hash_table_insert (created_files, g_object_ref (file), _g_file_info_create_from_entry (entry, extract_data));
				archive_read_data_skip (a);
				break;

			case AE_IFREG:
				ostream = (GOutputStream *) g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, cancellable, &load_data->error);
				if (ostream == NULL)
					break;

				actual_offset = 0;
				while ((r = archive_read_data_block (a, &buffer, &buffer_size, &target_offset)) == ARCHIVE_OK) {
					gsize bytes_written;

					if (target_offset > actual_offset) {
						if (! _g_output_stream_add_padding (extract_data, ostream, target_offset, actual_offset, cancellable, &load_data->error))
							break;
						fr_archive_progress_inc_completed_bytes (load_data->archive, target_offset - actual_offset);
						actual_offset = target_offset;
					}

					if (! g_output_stream_write_all (ostream, buffer, buffer_size, &bytes_written, cancellable, &load_data->error))
						break;

					actual_offset += bytes_written;
					fr_archive_progress_inc_completed_bytes (load_data->archive, bytes_written);
				}

				if ((r == ARCHIVE_EOF) && (target_offset > actual_offset))
					_g_output_stream_add_padding (extract_data, ostream, target_offset, actual_offset, cancellable, &load_data->error);

				_g_object_unref (ostream);

				if (r != ARCHIVE_EOF)
					load_data->error = _g_error_new_from_archive_error (archive_error_string (a));
				else
					g_hash_table_insert (created_files, g_object_ref (file), _g_file_info_create_from_entry (entry, extract_data));
				break;

			case AE_IFLNK:
				if (! g_file_make_symbolic_link (file, archive_entry_symlink (entry), cancellable, &local_error)) {
					if (! g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
						load_data->error = g_error_copy (local_error);
					g_clear_error (&local_error);
				}
				else if (_symlink_is_external_to_destination (file, archive_entry_symlink (entry), extract_data->destination, external_links))
					g_hash_table_insert (external_links, g_object_ref (file), GINT_TO_POINTER (1));
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

	if (load_data->error == NULL)
		restore_original_file_attributes (created_files, cancellable);

	if ((load_data->error == NULL) && (r != ARCHIVE_EOF))
		load_data->error = _g_error_new_from_archive_error (archive_error_string (a));
	if (load_data->error == NULL)
		g_cancellable_set_error_if_cancelled (cancellable, &load_data->error);
	if (load_data->error != NULL)
		g_simple_async_result_set_from_error (result, load_data->error);

	g_hash_table_unref (folders_created_during_extraction);
	g_hash_table_unref (created_files);
	g_hash_table_unref (checked_folders);
	g_hash_table_unref (external_links);
	archive_read_free (a);
	extract_data_free (extract_data);
}