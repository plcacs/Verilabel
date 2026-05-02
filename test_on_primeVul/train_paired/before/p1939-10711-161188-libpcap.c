daemon_msg_open_req(uint8 ver, struct daemon_slpars *pars, uint32 plen,
    char *source, size_t sourcelen)
{
	char errbuf[PCAP_ERRBUF_SIZE];		// buffer for network errors
	char errmsgbuf[PCAP_ERRBUF_SIZE];	// buffer for errors to send to the client
	pcap_t *fp;				// pcap_t main variable
	int nread;
	char sendbuf[RPCAP_NETBUF_SIZE];	// temporary buffer in which data to be sent is buffered
	int sendbufidx = 0;			// index which keeps the number of bytes currently buffered
	struct rpcap_openreply *openreply;	// open reply message

	if (plen > sourcelen - 1)
	{
		pcap_snprintf(errmsgbuf, PCAP_ERRBUF_SIZE, "Source string too long");
		goto error;
	}

	nread = sock_recv(pars->sockctrl, source, plen,
	    SOCK_RECEIVEALL_YES|SOCK_EOF_IS_ERROR, errbuf, PCAP_ERRBUF_SIZE);
	if (nread == -1)
	{
		rpcapd_log(LOGPRIO_ERROR, "Read from client failed: %s", errbuf);
		return -1;
	}
	source[nread] = '\0';
	plen -= nread;

	// XXX - make sure it's *not* a URL; we don't support opening
	// remote devices here.

	// Open the selected device
	// This is a fake open, since we do that only to get the needed parameters, then we close the device again
	if ((fp = pcap_open_live(source,
			1500 /* fake snaplen */,
			0 /* no promis */,
			1000 /* fake timeout */,
			errmsgbuf)) == NULL)
		goto error;

	// Now, I can send a RPCAP open reply message
	if (sock_bufferize(NULL, sizeof(struct rpcap_header), NULL, &sendbufidx,
	    RPCAP_NETBUF_SIZE, SOCKBUF_CHECKONLY, errmsgbuf, PCAP_ERRBUF_SIZE) == -1)
		goto error;

	rpcap_createhdr((struct rpcap_header *) sendbuf, ver,
	    RPCAP_MSG_OPEN_REPLY, 0, sizeof(struct rpcap_openreply));

	openreply = (struct rpcap_openreply *) &sendbuf[sendbufidx];

	if (sock_bufferize(NULL, sizeof(struct rpcap_openreply), NULL, &sendbufidx,
	    RPCAP_NETBUF_SIZE, SOCKBUF_CHECKONLY, errmsgbuf, PCAP_ERRBUF_SIZE) == -1)
		goto error;

	memset(openreply, 0, sizeof(struct rpcap_openreply));
	openreply->linktype = htonl(pcap_datalink(fp));
	openreply->tzoff = 0; /* This is always 0 for live captures */

	// We're done with the pcap_t.
	pcap_close(fp);

	// Send the reply.
	if (sock_send(pars->sockctrl, sendbuf, sendbufidx, errbuf, PCAP_ERRBUF_SIZE) == -1)
	{
		rpcapd_log(LOGPRIO_ERROR, "Send to client failed: %s", errbuf);
		return -1;
	}
	return 0;

error:
	if (rpcap_senderror(pars->sockctrl, ver, PCAP_ERR_OPEN,
	    errmsgbuf, errbuf) == -1)
	{
		// That failed; log a message and give up.
		rpcapd_log(LOGPRIO_ERROR, "Send to client failed: %s", errbuf);
		return -1;
	}

	// Check if all the data has been read; if not, discard the data in excess
	if (rpcapd_discard(pars->sockctrl, plen) == -1)
	{
		return -1;
	}
	return 0;
}