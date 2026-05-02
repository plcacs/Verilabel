HRESULT Http::HrReadHeaders()
{
	HRESULT hr;
	std::string strBuffer;
	ULONG n = 0;
	std::map<std::string, std::string>::iterator iHeader = mapHeaders.end();
	static constexpr std::size_t MAX_HEADER_LENGTH = 65536;
	std::size_t numOfBytesRead = 0;

	ec_log_debug("Receiving headers:");
	do
	{
		hr = m_lpChannel->HrReadLine(strBuffer);
		if (hr != hrSuccess)
			return hr;
		if (strBuffer.empty())
			break;

		numOfBytesRead += strBuffer.size();
		if(numOfBytesRead > MAX_HEADER_LENGTH) {
			return MAPI_E_TOO_BIG;
		}

		if (n == 0) {
			m_strAction = strBuffer;
		} else {
			auto pos = strBuffer.find(':');
			size_t start = 0;

			if (strBuffer[0] == ' ' || strBuffer[0] == '\t') {
				if (iHeader == mapHeaders.end())
					continue;
				// continue header
				while (strBuffer[start] == ' ' || strBuffer[start] == '\t')
					++start;
				iHeader->second += strBuffer.substr(start);
			} else {
				// new header
				auto r = mapHeaders.emplace(strBuffer.substr(0, pos), strBuffer.substr(pos + 2));
				iHeader = r.first;
			}
		}

		if (strBuffer.find("Authorization") != std::string::npos)
			ec_log_debug("< Authorization: <value hidden>");
		else
			ec_log_debug("< "+strBuffer);
		++n;
	} while(hr == hrSuccess);

	hr = HrParseHeaders();
	if (hr != hrSuccess)
		hr_ldebug(hr, "parsing headers failed");
	return hr;
}