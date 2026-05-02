gboolean textview_uri_security_check(TextView *textview, ClickableText *uri)
{
	gchar *visible_str;
	gboolean retval = TRUE;

	if (is_uri_string(uri->uri) == FALSE)
		return FALSE;

	visible_str = textview_get_visible_uri(textview, uri);
	if (visible_str == NULL)
		return TRUE;

	g_strstrip(visible_str);

	if (strcmp(visible_str, uri->uri) != 0 && is_uri_string(visible_str)) {
		gchar *uri_path;
		gchar *visible_uri_path;

		uri_path = get_uri_path(uri->uri);
		visible_uri_path = get_uri_path(visible_str);
		if (path_cmp(uri_path, visible_uri_path) != 0)
			retval = FALSE;
	}

	if (retval == FALSE) {
		gchar *msg;
		AlertValue aval;

		msg = g_markup_printf_escaped("%s\n\n"
						"<b>%s</b> %s\n\n"
						"<b>%s</b> %s\n\n"
						"%s",
						_("The real URL is different from the displayed URL."),
						_("Displayed URL:"), visible_str,
						_("Real URL:"), uri->uri,
						_("Open it anyway?"));
		aval = alertpanel_full(_("Phishing attempt warning"), msg,
				       GTK_STOCK_CANCEL, _("_Open URL"), NULL, ALERTFOCUS_FIRST,
							 FALSE, NULL, ALERT_WARNING);
		g_free(msg);
		if (aval == G_ALERTALTERNATE)
			retval = TRUE;
	}
	if (strlen(uri->uri) > get_uri_len(uri->uri))
		retval = FALSE;

	g_free(visible_str);

	return retval;
}