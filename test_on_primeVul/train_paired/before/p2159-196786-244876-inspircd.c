DNSResult DNS::GetResult()
{
	/* Fetch dns query response and decide where it belongs */
	DNSHeader header;
	DNSRequest *req;
	unsigned char buffer[sizeof(DNSHeader)];
	irc::sockets::sockaddrs from;
	memset(&from, 0, sizeof(from));
	socklen_t x = sizeof(from);

	int length = ServerInstance->SE->RecvFrom(this, (char*)buffer, sizeof(DNSHeader), 0, &from.sa, &x);

	/* Did we get the whole header? */
	if (length < 12)
	{
		ServerInstance->Logs->Log("RESOLVER",DEBUG,"GetResult didn't get a full packet (len=%d)", length);
		/* Nope - something screwed up. */
		return DNSResult(-1,"",0,"");
	}

	/* Check wether the reply came from a different DNS
	 * server to the one we sent it to, or the source-port
	 * is not 53.
	 * A user could in theory still spoof dns packets anyway
	 * but this is less trivial than just sending garbage
	 * to the server, which is possible without this check.
	 *
	 * -- Thanks jilles for pointing this one out.
	 */
	if (from != myserver)
	{
		std::string server1 = from.str();
		std::string server2 = myserver.str();
		ServerInstance->Logs->Log("RESOLVER",DEBUG,"Got a result from the wrong server! Bad NAT or DNS forging attempt? '%s' != '%s'",
			server1.c_str(), server2.c_str());
		return DNSResult(-1,"",0,"");
	}

	/* Put the read header info into a header class */
	DNS::FillHeader(&header,buffer,length - 12);

	/* Get the id of this request.
	 * Its a 16 bit value stored in two char's,
	 * so we use logic shifts to create the value.
	 */
	unsigned long this_id = header.id[1] + (header.id[0] << 8);

	/* Do we have a pending request matching this id? */
	if (!requests[this_id])
	{
		/* Somehow we got a DNS response for a request we never made... */
		ServerInstance->Logs->Log("RESOLVER",DEBUG,"Hmm, got a result that we didn't ask for (id=%lx). Ignoring.", this_id);
		return DNSResult(-1,"",0,"");
	}
	else
	{
		/* Remove the query from the list of pending queries */
		req = requests[this_id];
		requests[this_id] = NULL;
	}

	/* Inform the DNSRequest class that it has a result to be read.
	 * When its finished it will return a DNSInfo which is a pair of
	 * unsigned char* resource record data, and an error message.
	 */
	DNSInfo data = req->ResultIsReady(header, length);
	std::string resultstr;

	/* Check if we got a result, if we didnt, its an error */
	if (data.first == NULL)
	{
		/* An error.
		 * Mask the ID with the value of ERROR_MASK, so that
		 * the dns_deal_with_classes() function knows that its
		 * an error response and needs to be treated uniquely.
		 * Put the error message in the second field.
		 */
		std::string ro = req->orig;
		delete req;
		return DNSResult(this_id | ERROR_MASK, data.second, 0, ro);
	}
	else
	{
		unsigned long ttl = req->ttl;
		char formatted[128];

		/* Forward lookups come back as binary data. We must format them into ascii */
		switch (req->type)
		{
			case DNS_QUERY_A:
				snprintf(formatted,16,"%u.%u.%u.%u",data.first[0],data.first[1],data.first[2],data.first[3]);
				resultstr = formatted;
			break;

			case DNS_QUERY_AAAA:
			{
				if (!inet_ntop(AF_INET6, data.first, formatted, sizeof(formatted)))
				{
					std::string ro = req->orig;
					delete req;
					return DNSResult(this_id | ERROR_MASK, "inet_ntop() failed", 0, ro);
				}

				resultstr = formatted;

				/* Special case. Sending ::1 around between servers
				 * and to clients is dangerous, because the : on the
				 * start makes the client or server interpret the IP
				 * as the last parameter on the line with a value ":1".
				 */
				if (*formatted == ':')
					resultstr.insert(0, "0");
			}
			break;

			case DNS_QUERY_CNAME:
				/* Identical handling to PTR */

			case DNS_QUERY_PTR:
				/* Reverse lookups just come back as char* */
				resultstr = std::string((const char*)data.first);
			break;

			default:
			break;
		}

		/* Build the reply with the id and hostname/ip in it */
		std::string ro = req->orig;
		DNSResult result = DNSResult(this_id,resultstr,ttl,ro,req->type);
		delete req;
		return result;
	}
}