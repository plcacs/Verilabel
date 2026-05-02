	SilenceMessage(const std::string& mask, const std::string& flags)
		: ClientProtocol::Message("SILENCE")
	{
		PushParam(mask);
		PushParamRef(flags);
	}