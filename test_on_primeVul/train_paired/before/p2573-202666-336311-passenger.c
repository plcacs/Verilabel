Controller::constructHeaderForSessionProtocol(Request *req, char * restrict buffer,
	unsigned int &size, const SessionProtocolWorkingState &state, string delta_monotonic)
{
	char *pos = buffer;
	const char *end = buffer + size;

	pos += sizeof(boost::uint32_t);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("REQUEST_URI"));
	pos = appendData(pos, end, req->path.start->data, req->path.size);
	pos = appendData(pos, end, "", 1);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("PATH_INFO"));
	pos = appendData(pos, end, state.path.data(), state.path.size());
	pos = appendData(pos, end, "", 1);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("SCRIPT_NAME"));
	if (state.hasBaseURI) {
		pos = appendData(pos, end, req->options.baseURI);
		pos = appendData(pos, end, "", 1);
	} else {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL(""));
	}

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("QUERY_STRING"));
	pos = appendData(pos, end, state.queryString.data(), state.queryString.size());
	pos = appendData(pos, end, "", 1);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("REQUEST_METHOD"));
	pos = appendData(pos, end, state.methodStr);
	pos = appendData(pos, end, "", 1);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("SERVER_NAME"));
	pos = appendData(pos, end, state.serverName);
	pos = appendData(pos, end, "", 1);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("SERVER_PORT"));
	pos = appendData(pos, end, state.serverPort);
	pos = appendData(pos, end, "", 1);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("SERVER_SOFTWARE"));
	pos = appendData(pos, end, serverSoftware);
	pos = appendData(pos, end, "", 1);

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("SERVER_PROTOCOL"));
	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("HTTP/1.1"));

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("REMOTE_ADDR"));
	if (state.remoteAddr != NULL) {
		pos = appendData(pos, end, state.remoteAddr);
		pos = appendData(pos, end, "", 1);
	} else {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("127.0.0.1"));
	}

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("REMOTE_PORT"));
	if (state.remotePort != NULL) {
		pos = appendData(pos, end, state.remotePort);
		pos = appendData(pos, end, "", 1);
	} else {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("0"));
	}

	if (state.remoteUser != NULL) {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("REMOTE_USER"));
		pos = appendData(pos, end, state.remoteUser);
		pos = appendData(pos, end, "", 1);
	}

	if (state.contentType != NULL) {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("CONTENT_TYPE"));
		pos = appendData(pos, end, state.contentType);
		pos = appendData(pos, end, "", 1);
	}

	if (state.contentLength != NULL) {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("CONTENT_LENGTH"));
		pos = appendData(pos, end, state.contentLength);
		pos = appendData(pos, end, "", 1);
	}

	pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("PASSENGER_CONNECT_PASSWORD"));
	pos = appendData(pos, end, req->session->getApiKey().toStaticString());
	pos = appendData(pos, end, "", 1);

	if (req->https) {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("HTTPS"));
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("on"));
	}

	if (req->options.analytics) {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("PASSENGER_TXN_ID"));
		pos = appendData(pos, end, req->options.transaction->getTxnId());
		pos = appendData(pos, end, "", 1);

		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("PASSENGER_DELTA_MONOTONIC"));
		pos = appendData(pos, end, delta_monotonic);
		pos = appendData(pos, end, "", 1);
	}

	if (req->upgraded()) {
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("HTTP_CONNECTION"));
		pos = appendData(pos, end, P_STATIC_STRING_WITH_NULL("upgrade"));
	}

	ServerKit::HeaderTable::Iterator it(req->headers);
	while (*it != NULL) {
		if ((it->header->hash == HTTP_CONTENT_LENGTH.hash()
			|| it->header->hash == HTTP_CONTENT_TYPE.hash()
			|| it->header->hash == HTTP_CONNECTION.hash())
		 && (psg_lstr_cmp(&it->header->key, P_STATIC_STRING("content-type"))
			|| psg_lstr_cmp(&it->header->key, P_STATIC_STRING("content-length"))
			|| psg_lstr_cmp(&it->header->key, P_STATIC_STRING("connection"))))
		{
			it.next();
			continue;
		}

		pos = appendData(pos, end, P_STATIC_STRING("HTTP_"));
		const LString::Part *part = it->header->key.start;
		while (part != NULL) {
			char *start = pos;
			pos = appendData(pos, end, part->data, part->size);
			httpHeaderToScgiUpperCase((unsigned char *) start, pos - start);
			part = part->next;
		}
		pos = appendData(pos, end, "", 1);

		part = it->header->val.start;
		while (part != NULL) {
			pos = appendData(pos, end, part->data, part->size);
			part = part->next;
		}
		pos = appendData(pos, end, "", 1);

		it.next();
	}

	if (state.environmentVariablesData != NULL) {
		pos = appendData(pos, end, state.environmentVariablesData, state.environmentVariablesSize);
	}

	Uint32Message::generate(buffer, pos - buffer - sizeof(boost::uint32_t));

	size = pos - buffer;
	return pos < end;
}