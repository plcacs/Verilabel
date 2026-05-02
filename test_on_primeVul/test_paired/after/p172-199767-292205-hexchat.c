inbound_cap_ls (server *serv, char *nick, char *extensions_str,
					 const message_tags_data *tags_data)
{
	char buffer[256];	/* buffer for requesting capabilities and emitting the signal */
	gboolean want_cap = FALSE; /* format the CAP REQ string based on previous capabilities being requested or not */
	gboolean want_sasl = FALSE; /* CAP END shouldn't be sent when SASL is requested, it needs further responses */
	char **extensions;
	int i;

	EMIT_SIGNAL_TIMESTAMP (XP_TE_CAPLIST, serv->server_session, nick,
								  extensions_str, NULL, NULL, 0, tags_data->timestamp);

	extensions = g_strsplit (extensions_str, " ", 0);

	strcpy (buffer, "CAP REQ :");

	for (i=0; extensions[i]; i++)
	{
		const char *extension = extensions[i];
		gsize x;

		/* if the SASL password is set AND auth mode is set to SASL, request SASL auth */
		if (!g_strcmp0 (extension, "sasl") &&
			((serv->loginmethod == LOGIN_SASL && strlen (serv->password) != 0)
				|| (serv->loginmethod == LOGIN_SASLEXTERNAL && serv->have_cert)))
		{
			want_cap = TRUE;
			want_sasl = TRUE;
			g_strlcat (buffer, "sasl ", sizeof(buffer));
			continue;
		}

		for (x = 0; x < G_N_ELEMENTS(supported_caps); ++x)
		{
			if (!g_strcmp0 (extension, supported_caps[x]))
			{
				g_strlcat (buffer, extension, sizeof(buffer));
				g_strlcat (buffer, " ", sizeof(buffer));
				want_cap = TRUE;
			}
		}
	}

	g_strfreev (extensions);

	if (want_cap)
	{
		/* buffer + 9 = emit buffer without "CAP REQ :" */
		EMIT_SIGNAL_TIMESTAMP (XP_TE_CAPREQ, serv->server_session,
									  buffer + 9, NULL, NULL, NULL, 0,
									  tags_data->timestamp);
		tcp_sendf (serv, "%s\r\n", g_strchomp (buffer));
	}
	if (!want_sasl)
	{
		/* if we use SASL, CAP END is dealt via raw numerics */
		serv->sent_capend = TRUE;
		tcp_send_len (serv, "CAP END\r\n", 9);
	}
}