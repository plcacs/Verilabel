int shadow_server_start(rdpShadowServer* server)
{
	BOOL ipc;
	BOOL status;
	WSADATA wsaData;

	if (!server)
		return -1;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return -1;

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	server->screen = shadow_screen_new(server);

	if (!server->screen)
	{
		WLog_ERR(TAG, "screen_new failed");
		return -1;
	}

	server->capture = shadow_capture_new(server);

	if (!server->capture)
	{
		WLog_ERR(TAG, "capture_new failed");
		return -1;
	}

	/* Bind magic:
	 *
	 * emtpy                 ... bind TCP all
	 * <local path>          ... bind local (IPC)
	 * bind-socket,<address> ... bind TCP to specified interface
	 */
	ipc = server->ipcSocket && (strncmp(bind_address, server->ipcSocket,
	                                    strnlen(bind_address, sizeof(bind_address))) != 0);
	if (!ipc)
	{
		size_t x, count;
		char** list = CommandLineParseCommaSeparatedValuesEx(NULL, server->ipcSocket, &count);
		if (!list || (count <= 1))
		{
			if (server->ipcSocket == NULL)
			{
				if (!open_port(server, NULL))
				{
					free(list);
					return -1;
				}
			}
			else
			{
				free(list);
				return -1;
			}
		}

		for (x = 1; x < count; x++)
		{
			BOOL success = open_port(server, list[x]);
			if (!success)
			{
				free(list);
				return -1;
			}
		}
		free(list);
	}
	else
	{
		status = server->listener->OpenLocal(server->listener, server->ipcSocket);
		if (!status)
		{
			WLog_ERR(TAG, "Problem creating local socket listener. (Port already used or "
			              "insufficient permissions?)");
			return -1;
		}
	}

	if (!(server->thread = CreateThread(NULL, 0, shadow_server_thread, (void*)server, 0, NULL)))
	{
		return -1;
	}

	return 0;
}