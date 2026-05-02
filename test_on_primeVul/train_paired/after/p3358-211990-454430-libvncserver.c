HandleFileDownloadCancelRequest(rfbClientPtr cl, rfbTightClientPtr rtcp)
{
	int n = 0;
	char *reason = NULL;
	rfbClientToServerTightMsg msg;

	memset(&msg, 0, sizeof(rfbClientToServerTightMsg));
	
	if((n = rfbReadExact(cl, ((char *)&msg)+1, sz_rfbFileDownloadCancelMsg-1)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading "
					"FileDownloadCancelMsg\n", __FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    return;
	}

	msg.fdc.reasonLen = Swap16IfLE(msg.fdc.reasonLen);

	if(msg.fdc.reasonLen == 0) {
		rfbLog("File [%s]: Method [%s]: reason length received is Zero\n",
				__FILE__, __FUNCTION__);
		return;
	}
	
	reason = (char*) calloc(msg.fdc.reasonLen + 1, sizeof(char));
	if(reason == NULL) {
		rfbLog("File [%s]: Method [%s]: Fatal Error: Memory alloc failed\n", 
				__FILE__, __FUNCTION__);
		return;
	}

	if((n = rfbReadExact(cl, reason, msg.fdc.reasonLen)) <= 0) {
		
		if (n < 0)
			rfbLog("File [%s]: Method [%s]: Error while reading "
					"FileDownloadCancelMsg\n", __FILE__, __FUNCTION__);
		
	    rfbCloseClient(cl);
	    free(reason);
	    return;
	}

	rfbLog("File [%s]: Method [%s]: File Download Cancel Request received:"
					" reason <%s>\n", __FILE__, __FUNCTION__, reason);
	
	pthread_mutex_lock(&fileDownloadMutex);
	CloseUndoneFileTransfer(cl, rtcp);
	pthread_mutex_unlock(&fileDownloadMutex);
	
	if(reason != NULL) {
		free(reason);
		reason = NULL;
	}

}