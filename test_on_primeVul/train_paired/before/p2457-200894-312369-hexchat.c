scrollback_get_filename (session *sess)
{
	char *net, *chan, *buf, *ret = NULL;

	net = server_get_network (sess->server, FALSE);
	if (!net)
		return NULL;

	buf = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "scrollback" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s.txt", get_xdir (), net, "");
	mkdir_p (buf);
	g_free (buf);

	chan = log_create_filename (sess->channel);
	if (chan[0])
		buf = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "scrollback" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s.txt", get_xdir (), net, chan);
	else
		buf = NULL;
	g_free (chan);

	if (buf)
	{
		ret = g_filename_from_utf8 (buf, -1, NULL, NULL, NULL);
		g_free (buf);
	}

	return ret;
}