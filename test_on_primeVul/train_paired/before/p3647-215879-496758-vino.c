rfbProcessClientMessage(rfbClientPtr cl)
{
    switch (cl->state) {
    case RFB_PROTOCOL_VERSION:
        rfbProcessClientProtocolVersion(cl);
        return;
    case RFB_SECURITY_TYPE:
	rfbAuthProcessSecurityTypeMessage(cl);
	return;
#ifdef VINO_HAVE_GNUTLS
    case RFB_TLS_HANDSHAKE:
	rfbAuthProcessTLSHandshake(cl);
	return;
#endif
    case RFB_AUTH_TYPE:
	rfbAuthProcessAuthTypeMessage(cl);
	return;
    case RFB_AUTHENTICATION:
        rfbAuthProcessClientMessage(cl);
        return;
    case RFB_AUTH_DEFERRED:
	rfbLog("Authentication deferred - ignoring client message\n");
	return;
    case RFB_INITIALISATION:
        rfbProcessClientInitMessage(cl);
        return;
    default:
        rfbProcessClientNormalMessage(cl);
        return;
    }
}