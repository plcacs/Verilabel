mail_parser_run (EMailParser *parser,
                 EMailPartList *part_list,
                 GCancellable *cancellable)
{
	EMailExtensionRegistry *reg;
	CamelMimeMessage *message;
	EMailPart *mail_part;
	GQueue *parsers;
	GQueue mail_part_queue = G_QUEUE_INIT;
	GList *iter;
	GString *part_id;

	if (cancellable)
		g_object_ref (cancellable);
	else
		cancellable = g_cancellable_new ();

	g_mutex_lock (&parser->priv->mutex);
	g_hash_table_insert (parser->priv->ongoing_part_lists, cancellable, part_list);
	g_mutex_unlock (&parser->priv->mutex);

	message = e_mail_part_list_get_message (part_list);

	reg = e_mail_parser_get_extension_registry (parser);

	parsers = e_mail_extension_registry_get_for_mime_type (
		reg, "application/vnd.evolution.message");

	if (parsers == NULL)
		parsers = e_mail_extension_registry_get_for_mime_type (
			reg, "message/*");

	/* No parsers means the internal Evolution parser
	 * extensions were not loaded. Something is terribly wrong! */
	g_return_if_fail (parsers != NULL);

	part_id = g_string_new (".message");

	mail_part = e_mail_part_new (CAMEL_MIME_PART (message), ".message");
	e_mail_part_list_add_part (part_list, mail_part);
	g_object_unref (mail_part);

	for (iter = parsers->head; iter; iter = iter->next) {
		EMailParserExtension *extension;
		gboolean message_handled;

		if (g_cancellable_is_cancelled (cancellable))
			break;

		extension = iter->data;
		if (!extension)
			continue;

		message_handled = e_mail_parser_extension_parse (
			extension, parser,
			CAMEL_MIME_PART (message),
			part_id, cancellable, &mail_part_queue);

		if (message_handled)
			break;
	}

	while (!g_queue_is_empty (&mail_part_queue)) {
		mail_part = g_queue_pop_head (&mail_part_queue);
		e_mail_part_list_add_part (part_list, mail_part);
		g_object_unref (mail_part);
	}

	g_mutex_lock (&parser->priv->mutex);
	g_hash_table_remove (parser->priv->ongoing_part_lists, cancellable);
	g_mutex_unlock (&parser->priv->mutex);

	g_clear_object (&cancellable);
	g_string_free (part_id, TRUE);
}