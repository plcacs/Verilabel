	Pong(const std::string& cookie, const std::string& server = "")
		: ClientProtocol::Message("PONG", ServerInstance->Config->GetServerName())
	{
		PushParamRef(ServerInstance->Config->GetServerName());
		if (!server.empty())
			PushParamRef(server);
		PushParamRef(cookie);
	}