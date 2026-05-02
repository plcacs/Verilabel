	Pong(const std::string& cookie, const std::string& server = "")
		: ClientProtocol::Message("PONG", ServerInstance->Config->GetServerName())
	{
		if (server.empty())
			PushParamRef(ServerInstance->Config->GetServerName());
		else
			PushParam(server);
		PushParamRef(cookie);
	}