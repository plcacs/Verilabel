imapx_connect_to_server (CamelIMAPXServer *is,
                         GCancellable *cancellable,
                         GError **error)
{
	CamelNetworkSettings *network_settings;
	CamelNetworkSecurityMethod method;
	CamelIMAPXStore *store;
	CamelSettings *settings;
	GIOStream *connection = NULL;
	GIOStream *tls_stream;
	GSocket *socket;
	guint len;
	guchar *token;
	gint tok;
	CamelIMAPXCommand *ic;
	gchar *shell_command = NULL;
	gboolean use_shell_command;
	gboolean success = TRUE;
	gchar *host;

	store = camel_imapx_server_ref_store (is);

	settings = camel_service_ref_settings (CAMEL_SERVICE (store));

	network_settings = CAMEL_NETWORK_SETTINGS (settings);
	host = camel_network_settings_dup_host (network_settings);
	method = camel_network_settings_get_security_method (network_settings);

	use_shell_command = camel_imapx_settings_get_use_shell_command (
		CAMEL_IMAPX_SETTINGS (settings));

	if (use_shell_command)
		shell_command = camel_imapx_settings_dup_shell_command (
			CAMEL_IMAPX_SETTINGS (settings));

	g_object_unref (settings);

	if (shell_command != NULL) {
		success = connect_to_server_process (is, shell_command, error);

		g_free (shell_command);

		if (success)
			goto connected;
		else
			goto exit;
	}

	connection = camel_network_service_connect_sync (
		CAMEL_NETWORK_SERVICE (store), cancellable, error);

	if (connection != NULL) {
		GInputStream *input_stream;
		GOutputStream *output_stream;
		GError *local_error = NULL;

		/* Disable the Nagle algorithm with TCP_NODELAY, since IMAP
		 * commands should be issued immediately even we've not yet
		 * received a response to a previous command. */
		socket = g_socket_connection_get_socket (
			G_SOCKET_CONNECTION (connection));
		g_socket_set_option (
			socket, IPPROTO_TCP, TCP_NODELAY, 1, &local_error);
		if (local_error != NULL) {
			/* Failure to set the socket option is non-fatal. */
			g_warning ("%s: %s", G_STRFUNC, local_error->message);
			g_clear_error (&local_error);
		}

		g_mutex_lock (&is->priv->stream_lock);
		g_warn_if_fail (is->priv->connection == NULL);
		is->priv->connection = g_object_ref (connection);
		g_mutex_unlock (&is->priv->stream_lock);

		input_stream = g_io_stream_get_input_stream (connection);
		output_stream = g_io_stream_get_output_stream (connection);

		imapx_server_set_streams (is, input_stream, output_stream);

		/* Hang on to the connection reference in case we need to
		 * issue STARTTLS below. */
	} else {
		success = FALSE;
		goto exit;
	}

connected:
	while (1) {
		GInputStream *input_stream;

		input_stream = camel_imapx_server_ref_input_stream (is);

		token = NULL;
		tok = camel_imapx_input_stream_token (
			CAMEL_IMAPX_INPUT_STREAM (input_stream),
			&token, &len, cancellable, error);

		if (tok < 0) {
			success = FALSE;

		} else if (tok == '*') {
			success = imapx_untagged (
				is, input_stream, cancellable, error);

			if (success) {
				g_object_unref (input_stream);
				break;
			}

		} else {
			camel_imapx_input_stream_ungettoken (
				CAMEL_IMAPX_INPUT_STREAM (input_stream),
				tok, token, len);

			success = camel_imapx_input_stream_text (
				CAMEL_IMAPX_INPUT_STREAM (input_stream),
				&token, cancellable, error);

			g_free (token);
		}

		g_object_unref (input_stream);

		if (!success)
			goto exit;
	}

	g_mutex_lock (&is->priv->stream_lock);

	if (!is->priv->cinfo) {
		g_mutex_unlock (&is->priv->stream_lock);

		ic = camel_imapx_command_new (is, CAMEL_IMAPX_JOB_CAPABILITY, "CAPABILITY");

		success = camel_imapx_server_process_command_sync (is, ic, _("Failed to get capabilities"), cancellable, error);

		camel_imapx_command_unref (ic);

		if (!success)
			goto exit;
	} else {
		g_mutex_unlock (&is->priv->stream_lock);
	}

	if (method == CAMEL_NETWORK_SECURITY_METHOD_STARTTLS_ON_STANDARD_PORT) {

		g_mutex_lock (&is->priv->stream_lock);

		if (CAMEL_IMAPX_LACK_CAPABILITY (is->priv->cinfo, STARTTLS)) {
			g_mutex_unlock (&is->priv->stream_lock);
			g_set_error (
				error, CAMEL_ERROR,
				CAMEL_ERROR_GENERIC,
				_("Failed to connect to IMAP server %s in secure mode: %s"),
				host, _("STARTTLS not supported"));
			success = FALSE;
			goto exit;
		} else {
			g_mutex_unlock (&is->priv->stream_lock);
		}

		ic = camel_imapx_command_new (is, CAMEL_IMAPX_JOB_STARTTLS, "STARTTLS");

		success = camel_imapx_server_process_command_sync (is, ic, _("Failed to issue STARTTLS"), cancellable, error);

		if (success) {
			g_mutex_lock (&is->priv->stream_lock);

			/* See if we got new capabilities
			 * in the STARTTLS response. */
			imapx_free_capability (is->priv->cinfo);
			is->priv->cinfo = NULL;
			if (ic->status->condition == IMAPX_CAPABILITY) {
				is->priv->cinfo = ic->status->u.cinfo;
				ic->status->u.cinfo = NULL;
				c (is->priv->tagprefix, "got capability flags %08x\n", is->priv->cinfo ? is->priv->cinfo->capa : 0xFFFFFFFF);
				imapx_server_stash_command_arguments (is);
			}

			g_mutex_unlock (&is->priv->stream_lock);
		}

		camel_imapx_command_unref (ic);

		if (!success)
			goto exit;

		tls_stream = camel_network_service_starttls (
			CAMEL_NETWORK_SERVICE (store), connection, error);

		if (tls_stream != NULL) {
			GInputStream *input_stream;
			GOutputStream *output_stream;

			g_mutex_lock (&is->priv->stream_lock);
			g_object_unref (is->priv->connection);
			is->priv->connection = g_object_ref (tls_stream);
			g_mutex_unlock (&is->priv->stream_lock);

			input_stream =
				g_io_stream_get_input_stream (tls_stream);
			output_stream =
				g_io_stream_get_output_stream (tls_stream);

			imapx_server_set_streams (
				is, input_stream, output_stream);

			g_object_unref (tls_stream);
		} else {
			g_prefix_error (
				error,
				_("Failed to connect to IMAP server %s in secure mode: "),
				host);
			success = FALSE;
			goto exit;
		}

		/* Get new capabilities if they weren't already given */
		g_mutex_lock (&is->priv->stream_lock);
		if (is->priv->cinfo == NULL) {
			g_mutex_unlock (&is->priv->stream_lock);
			ic = camel_imapx_command_new (is, CAMEL_IMAPX_JOB_CAPABILITY, "CAPABILITY");
			success = camel_imapx_server_process_command_sync (is, ic, _("Failed to get capabilities"), cancellable, error);
			camel_imapx_command_unref (ic);

			if (!success)
				goto exit;
		} else {
			g_mutex_unlock (&is->priv->stream_lock);
		}
	}

exit:
	if (!success) {
		g_mutex_lock (&is->priv->stream_lock);

		g_clear_object (&is->priv->input_stream);
		g_clear_object (&is->priv->output_stream);
		g_clear_object (&is->priv->connection);
		g_clear_object (&is->priv->subprocess);

		if (is->priv->cinfo != NULL) {
			imapx_free_capability (is->priv->cinfo);
			is->priv->cinfo = NULL;
		}

		g_mutex_unlock (&is->priv->stream_lock);
	}

	g_free (host);

	g_clear_object (&connection);
	g_clear_object (&store);

	return success;
}