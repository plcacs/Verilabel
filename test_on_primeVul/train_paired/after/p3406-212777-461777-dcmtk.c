int main(int argc, char *argv[])
{

#ifdef HAVE_GUSI_H
    GUSISetup(GUSIwithSIOUXSockets);
    GUSISetup(GUSIwithInternetSockets);
#endif

#ifdef WITH_TCPWRAPPER
    // this code makes sure that the linker cannot optimize away
    // the DUL part of the network module where the external flags
    // for libwrap are defined. Needed on OpenBSD.
    dcmTCPWrapperDaemonName.set(NULL);
#endif

#ifdef HAVE_WINSOCK_H
    WSAData winSockData;
    /* we need at least version 1.1 */
    WORD winSockVersionNeeded = MAKEWORD( 1, 1 );
    WSAStartup(winSockVersionNeeded, &winSockData);
#endif

    OFCmdUnsignedInt opt_port = 0;                   /* listen port */
    Uint32           clientID = 0;                   /* IDs assigned to connecting clients */

    OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION , "Sample message server for class DVPSIPCClient", rcsid);
    OFCommandLine cmd;
    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.setParamColumn(LONGCOL + SHORTCOL + 2);

    cmd.addParam("port", "port number to listen at");

    cmd.addGroup("general options:", LONGCOL, SHORTCOL);
      cmd.addOption("--help", "-h", "print this help text and exit", OFCommandLine::AF_Exclusive);
      OFLog::addOptions(cmd);

    /* evaluate command line */
    prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);
    if (app.parseCommandLine(cmd, argc, argv))
    {
      cmd.getParam(1, opt_port);
      OFLog::configureFromCommandLine(cmd, app);
    }

    OFLOG_DEBUG(msgservLogger, rcsid << OFendl);

    unsigned short networkPort = (unsigned short) opt_port;
    if (networkPort==0)
    {
      OFLOG_FATAL(msgservLogger, "no or invalid port number");
      return 10;
    }

#ifndef DISABLE_PORT_PERMISSION_CHECK
#ifdef HAVE_GETEUID
    /* if port is privileged we must be as well */
    if ((networkPort < 1024)&&(geteuid() != 0))
    {
      OFLOG_FATAL(msgservLogger, "cannot listen on port " << networkPort << ", insufficient privileges");
      return 10;
    }
#endif
#endif

    /* open listen socket */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
      OFLOG_FATAL(msgservLogger, "failed to create socket");
      return 10;
    }

#ifdef HAVE_GUSI_H
    /* GUSI always returns an error for setsockopt(...) */
#else
    int reuse = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse)) < 0)
    {
      OFLOG_FATAL(msgservLogger, "failed to set socket options");
      return 10;
    }
#endif

    /* Name socket using wildcards */
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = (unsigned short) htons(networkPort);
    if (bind(s, (struct sockaddr *) & server, sizeof(server)))
    {
      OFLOG_FATAL(msgservLogger, "failed to bind socket to port, already in use?");
      return 10;
    }
    listen(s, 64);  // accept max 64 pending TCP connections on this socket

#if defined(HAVE_SETUID) && defined(HAVE_GETUID)
      /* return to normal uid so that we can't do too much damage in case
       * things go very wrong.   Only relevant if the program is setuid root,
       * and run by another user.  Running as root user may be
       * potentially disasterous if this program screws up badly.
       */
      if ((setuid(getuid()) == -1) && (errno == EAGAIN))
      {
          OFLOG_FATAL(msgservLogger, "setuid() failed, maximum number of processes/threads for uid already running.");
          return 10;
      }
#endif

    fd_set fdset;
    struct timeval t;
    int nfound;
    while (1)
    {
      // wait for next incoming connection
      FD_ZERO(&fdset);
      FD_SET(s, &fdset);
      t.tv_sec = 10;  // 10 seconds timeout
      t.tv_usec = 0;

#ifdef HAVE_INTP_SELECT
      nfound = select(s + 1, (int *)(&fdset), NULL, NULL, &t);
#else
      nfound = select(s + 1, &fdset, NULL, NULL, &t);
#endif


      if (nfound > 0)
      {
        // incoming connection detected
        int sock=0;
        struct sockaddr from;
#ifdef HAVE_DECLARATION_SOCKLEN_T
        socklen_t len = sizeof(from);
#elif !defined(HAVE_PROTOTYPE_ACCEPT) || defined(HAVE_INTP_ACCEPT)
        int len = sizeof(from);
#else
        size_t len = sizeof(from);
#endif
        do
        {
          sock = accept(s, &from, &len);
        } while ((sock == -1)&&(errno == EINTR));

        if (sock < 0)
        {
          OFLOG_FATAL(msgservLogger, "unable to accept incoming connection");
          return 10;
        }

#ifdef HAVE_GUSI_H
        /* GUSI always returns an error for setsockopt(...) */
#else
        reuse = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse)) < 0)
        {
          OFLOG_FATAL(msgservLogger, "failed to set socket options");
          return 10;
        }
#endif

        // now we can handle the incoming connection
        DcmTCPConnection connection(sock);
        DVPSIPCMessage msg;
        Uint32 i=0;
        Uint32 msgType=0;
        OFString str;

        OFBool finished = OFFalse;
        while (!finished)
        {
          while (! connection.networkDataAvailable(1))
          {
            // waiting for network data to become available or connection to be closed
          }
          if (msg.receive(connection))
          {
            OFOStringStream oss;
            // handle message
            msgType = msg.getMessageType();
            if (msgType == DVPSIPCMessage::OK)
            {
                oss << "received 'OK' (should not happen)" << OFendl;
            } else if (msgType == DVPSIPCMessage::requestApplicationID) {
                oss << "New client requests application ID, assigning #" << clientID+1 << OFendl
                     << "Application Type: ";
                if (msg.extractIntFromPayload(i)) oss << applicationType(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::assignApplicationID) {
                oss << "received 'AssignApplicationID' (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::applicationTerminates) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Application Terminates, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
            } else if (msgType == DVPSIPCMessage::receivedUnencryptedDICOMConnection) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Received Unencrypted DICOM Connection, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::receivedEncryptedDICOMConnection) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Received Encrypted DICOM Connection, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::connectionClosed) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Connection Closed, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
            } else if (msgType == DVPSIPCMessage::connectionAborted) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Connection Aborted, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::requestedUnencryptedDICOMConnection) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Requested Unencrypted DICOM Connection, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::requestedEncryptedDICOMConnection) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Requested Encrypted DICOM Connection, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::receivedDICOMObject) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Received DICOM Object, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else if (msgType == DVPSIPCMessage::sentDICOMObject) {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "Sent DICOM Object, status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
                if (msg.extractStringFromPayload(str)) oss << str << OFendl; else oss << "No description (should not happen)." << OFendl;
            } else {
                if (msg.extractIntFromPayload(i)) oss << "#" << i << ": "; else oss << "unknown client: ";
                oss << "received unknown message type " << msg.getMessageType() << ", status: ";
                if (msg.extractIntFromPayload(i)) oss << statusString(i) << OFendl; else oss << "(none)" << OFendl;
            }
            oss << OFStringStream_ends;
            OFSTRINGSTREAM_GETSTR(oss, result);
            OFLOG_INFO(msgservLogger, result);
            OFSTRINGSTREAM_FREESTR(result);
            msg.erasePayload();
            if (msg.getMessageType() == DVPSIPCMessage::requestApplicationID)
            {
              msg.setMessageType(DVPSIPCMessage::assignApplicationID);
              msg.addIntToPayload(++clientID);
            } else {
              msg.setMessageType(DVPSIPCMessage::OK);
            }
            if (! msg.send(connection))
            {
              OFLOG_WARN(msgservLogger, "unable to send response message, closing connection");
              finished = OFTrue;
            }
          } else finished = OFTrue;
        }
        // connection has been closed by the client or something has gone wrong.
        // clean up connection and wait for next client.
        connection.close();
      }
    }

#ifdef HAVE_WINSOCK_H
    WSACleanup();
#endif

    return 0;
}