_fr_window_ask_overwrite_dialog (OverwriteData *odata)
{
	if ((odata->edata->overwrite == FR_OVERWRITE_ASK) && (odata->current_file != NULL)) {
		const char *base_name;
		GFile      *destination;

		base_name = _g_path_get_relative_basename ((char *) odata->current_file->data, odata->edata->base_dir, odata->edata->junk_paths);
		destination = g_file_get_child (odata->edata->destination, base_name);
		g_file_query_info_async (destination,
					 G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
					 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					 G_PRIORITY_DEFAULT,
					 odata->window->priv->cancellable,
					 query_info_ready_for_overwrite_dialog_cb,
					 odata);

		g_object_unref (destination);

		return;
	}

	if (odata->edata->file_list != NULL) {
		/* speed optimization: passing NULL when extracting all the
		 * files is faster if the command supports the
		 * propCanExtractAll property. */
		if (odata->extract_all) {
			_g_string_list_free (odata->edata->file_list);
			odata->edata->file_list = NULL;
		}
		odata->edata->overwrite = FR_OVERWRITE_YES;
		_fr_window_archive_extract_from_edata (odata->window, odata->edata);
	}
	else {
		GtkWidget *d;

		d = _gtk_message_dialog_new (GTK_WINDOW (odata->window),
					     0,
					     GTK_STOCK_DIALOG_WARNING,
					     _("Extraction not performed"),
					     NULL,
					     GTK_STOCK_OK, GTK_RESPONSE_OK,
					     NULL);
		gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);
		fr_window_show_error_dialog (odata->window, d, GTK_WINDOW (odata->window), _("Extraction not performed"));

		fr_window_stop_batch (odata->window);
	}

	g_free (odata);
}