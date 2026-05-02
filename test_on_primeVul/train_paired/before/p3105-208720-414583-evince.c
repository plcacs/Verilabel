dvi_document_file_exporter_end (EvFileExporter *exporter)
{
	gchar *command_line;
	gint exit_stat;
	GError *err = NULL;
	gboolean success;
	
	DviDocument *dvi_document = DVI_DOCUMENT(exporter);
	
	command_line = g_strdup_printf ("dvipdfm %s -o %s \"%s\"", /* dvipdfm -s 1,2,.., -o exporter_filename dvi_filename */
					dvi_document->exporter_opts->str,
					dvi_document->exporter_filename,
					dvi_document->context->filename);
	
	success = g_spawn_command_line_sync (command_line,
					     NULL,
					     NULL,
					     &exit_stat,
					     &err);

	g_free (command_line);

	if (success == FALSE) {
		g_warning ("Error: %s", err->message);
	} else if (!WIFEXITED(exit_stat) || WEXITSTATUS(exit_stat) != EXIT_SUCCESS){
		g_warning ("Error: dvipdfm does not end normally or exit with a failure status.");
	}

	if (err)
		g_error_free (err);
}