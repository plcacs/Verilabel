int main(int argc, char *argv[])
{

#ifdef HAVE_GUSI_H
    GUSISetup(GUSIwithSIOUXSockets);
    GUSISetup(GUSIwithInternetSockets);
#endif

#ifdef HAVE_WINSOCK_H
    WSAData winSockData;
    /* we need at least version 1.1 */
    WORD winSockVersionNeeded = MAKEWORD( 1, 1 );
    WSAStartup(winSockVersionNeeded, &winSockData);
#endif

    int         opt_terminate = 0;         /* default: no terminate mode */
    const char *opt_cfgName   = NULL;      /* config file name */
    const char *opt_cfgID     = NULL;      /* name of entry in config file */

    dcmDisableGethostbyaddr.set(OFTrue);               // disable hostname lookup

    OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION , "Network receive for presentation state viewer", rcsid);
    OFCommandLine cmd;
    cmd.setOptionColumns(LONGCOL, SHORTCOL);
    cmd.setParamColumn(LONGCOL + SHORTCOL + 2);

    cmd.addParam("config-file",  "configuration file to be read");
    cmd.addParam("receiver-id",  "identifier of receiver in config file", OFCmdParam::PM_Optional);

    cmd.addGroup("general options:");
     cmd.addOption("--help",      "-h", "print this help text and exit", OFCommandLine::AF_Exclusive);
     cmd.addOption("--version",         "print version information and exit", OFCommandLine::AF_Exclusive);
     OFLog::addOptions(cmd);
     cmd.addOption("--terminate", "-t", "terminate all running receivers");

    /* evaluate command line */
    prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);
    if (app.parseCommandLine(cmd, argc, argv))
    {
      /* check exclusive options first */
      if (cmd.hasExclusiveOption())
      {
        if (cmd.findOption("--version"))
        {
            app.printHeader(OFTrue /*print host identifier*/);
            COUT << OFendl << "External libraries used:";
#if !defined(WITH_ZLIB) && !defined(WITH_OPENSSL)
            COUT << " none" << OFendl;
#else
            COUT << OFendl;
#endif
#ifdef WITH_ZLIB
            COUT << "- ZLIB, Version " << zlibVersion() << OFendl;
#endif
#ifdef WITH_OPENSSL
            COUT << "- " << OPENSSL_VERSION_TEXT << OFendl;
#endif
            return 0;
         }
      }

      /* command line parameters and options */
      cmd.getParam(1, opt_cfgName);
      if (cmd.getParamCount() >= 2) cmd.getParam(2, opt_cfgID);
      if (cmd.findOption("--terminate")) opt_terminate = 1;

      OFLog::configureFromCommandLine(cmd, app);
    }

    /* print resource identifier */
    OFLOG_DEBUG(dcmpsrcvLogger, rcsid << OFendl);

    if ((opt_cfgID == 0)&&(! opt_terminate))
    {
      OFLOG_FATAL(dcmpsrcvLogger, "parameter receiver-id required unless --terminate is specified");
      return 10;
    }

    if (opt_cfgName)
    {
      FILE *cfgfile = fopen(opt_cfgName, "rb");
      if (cfgfile) fclose(cfgfile); else
      {
        OFLOG_FATAL(dcmpsrcvLogger, "can't open configuration file '" << opt_cfgName << "'");
        return 10;
      }
    } else {
      OFLOG_FATAL(dcmpsrcvLogger, "missing configuration file name");
      return 10;
    }

    /* make sure data dictionary is loaded */
    if (!dcmDataDict.isDictionaryLoaded())
    {
      OFLOG_WARN(dcmpsrcvLogger, "no data dictionary loaded, check environment variable: " << DCM_DICT_ENVIRONMENT_VARIABLE);
    }

    DVConfiguration dvi(opt_cfgName);
    if (opt_terminate)
    {
      terminateAllReceivers(dvi);
      return 0;  // application terminates here
    }

    /* get network configuration from configuration file */
    OFBool networkImplicitVROnly  = dvi.getTargetImplicitOnly(opt_cfgID);
    OFBool networkBitPreserving   = dvi.getTargetBitPreservingMode(opt_cfgID);
    OFBool opt_correctUIDPadding  = dvi.getTargetCorrectUIDPadding(opt_cfgID);
    OFBool networkDisableNewVRs   = dvi.getTargetDisableNewVRs(opt_cfgID);
    unsigned short networkPort    = dvi.getTargetPort(opt_cfgID);
    unsigned long  networkMaxPDU  = dvi.getTargetMaxPDU(opt_cfgID);
    const char *networkAETitle    = dvi.getTargetAETitle(opt_cfgID);
    if (networkAETitle==NULL) networkAETitle = dvi.getNetworkAETitle();
    unsigned short messagePort    = dvi.getMessagePort();   /* port number for IPC */
    OFBool keepMessagePortOpen    = dvi.getMessagePortKeepOpen();
    OFBool useTLS = dvi.getTargetUseTLS(opt_cfgID);
    OFBool notifyTermination      = OFTrue;  // notify IPC server of application termination
#ifdef WITH_OPENSSL
    /* TLS directory */
    const char *current = NULL;
    const char *tlsFolder = dvi.getTLSFolder();
    if (tlsFolder==NULL) tlsFolder = ".";

    /* certificate file */
    OFString tlsCertificateFile(tlsFolder);
    tlsCertificateFile += PATH_SEPARATOR;
    current = dvi.getTargetCertificate(opt_cfgID);
    if (current) tlsCertificateFile += current; else tlsCertificateFile += "sitecert.pem";

    /* private key file */
    OFString tlsPrivateKeyFile(tlsFolder);
    tlsPrivateKeyFile += PATH_SEPARATOR;
    current = dvi.getTargetPrivateKey(opt_cfgID);
    if (current) tlsPrivateKeyFile += current; else tlsPrivateKeyFile += "sitekey.pem";

    /* private key password */
    const char *tlsPrivateKeyPassword = dvi.getTargetPrivateKeyPassword(opt_cfgID);

    /* certificate verification */
    DcmCertificateVerification tlsCertVerification = DCV_requireCertificate;
    switch (dvi.getTargetPeerAuthentication(opt_cfgID))
    {
      case DVPSQ_require:
        tlsCertVerification = DCV_requireCertificate;
        break;
      case DVPSQ_verify:
        tlsCertVerification = DCV_checkCertificate;
        break;
      case DVPSQ_ignore:
        tlsCertVerification = DCV_ignoreCertificate;
        break;
    }

    /* DH parameter file */
    OFString tlsDHParametersFile;
    current = dvi.getTargetDiffieHellmanParameters(opt_cfgID);
    if (current)
    {
      tlsDHParametersFile = tlsFolder;
      tlsDHParametersFile += PATH_SEPARATOR;
      tlsDHParametersFile += current;
    }

    /* random seed file */
    OFString tlsRandomSeedFile(tlsFolder);
    tlsRandomSeedFile += PATH_SEPARATOR;
    current = dvi.getTargetRandomSeed(opt_cfgID);
    if (current) tlsRandomSeedFile += current; else tlsRandomSeedFile += "siteseed.bin";

    /* CA certificate directory */
    const char *tlsCACertificateFolder = dvi.getTLSCACertificateFolder();
    if (tlsCACertificateFolder==NULL) tlsCACertificateFolder = ".";

    /* key file format */
    int keyFileFormat = SSL_FILETYPE_PEM;
    if (! dvi.getTLSPEMFormat()) keyFileFormat = SSL_FILETYPE_ASN1;

    /* ciphersuites */
#if OPENSSL_VERSION_NUMBER >= 0x0090700fL
    OFString tlsCiphersuites(TLS1_TXT_RSA_WITH_AES_128_SHA ":" SSL3_TXT_RSA_DES_192_CBC3_SHA);
#else
    OFString tlsCiphersuites(SSL3_TXT_RSA_DES_192_CBC3_SHA);
#endif
    Uint32 tlsNumberOfCiphersuites = dvi.getTargetNumberOfCipherSuites(opt_cfgID);
    if (tlsNumberOfCiphersuites > 0)
    {
      tlsCiphersuites.clear();
      OFString currentSuite;
      const char *currentOpenSSL;
      for (Uint32 ui=0; ui<tlsNumberOfCiphersuites; ui++)
      {
        dvi.getTargetCipherSuite(opt_cfgID, ui, currentSuite);
        if (NULL == (currentOpenSSL = DcmTLSTransportLayer::findOpenSSLCipherSuiteName(currentSuite.c_str())))
        {
          OFLOG_FATAL(dcmpsrcvLogger, "ciphersuite '" << currentSuite << "' is unknown. Known ciphersuites are:");
          unsigned long numSuites = DcmTLSTransportLayer::getNumberOfCipherSuites();
          for (unsigned long cs=0; cs < numSuites; cs++)
          {
            OFLOG_FATAL(dcmpsrcvLogger, "    " << DcmTLSTransportLayer::getTLSCipherSuiteName(cs));
          }
          return 1;
        } else {
          if (!tlsCiphersuites.empty()) tlsCiphersuites += ":";
          tlsCiphersuites += currentOpenSSL;
        }
      }
    }
#else
    if (useTLS)
    {
      OFLOG_FATAL(dcmpsrcvLogger, "not compiled with OpenSSL, cannot use TLS");
      return 10;
    }
#endif

    if (networkAETitle==NULL)
    {
      OFLOG_FATAL(dcmpsrcvLogger, "no application entity title");
      return 10;
    }

    if (networkPort==0)
    {
      OFLOG_FATAL(dcmpsrcvLogger, "no or invalid port number");
      return 10;
    }

#ifndef DISABLE_PORT_PERMISSION_CHECK
#ifdef HAVE_GETEUID
    /* if port is privileged we must be as well */
    if ((networkPort < 1024)&&(geteuid() != 0))
    {
      OFLOG_FATAL(dcmpsrcvLogger, "cannot listen on port " << networkPort << ", insufficient privileges");
      return 10;
    }
#endif
#endif

    if (networkMaxPDU==0) networkMaxPDU = DEFAULT_MAXPDU;
    else if (networkMaxPDU > ASC_MAXIMUMPDUSIZE)
    {
      OFLOG_FATAL(dcmpsrcvLogger, "max PDU size " << networkMaxPDU << " too big, using default: " << DEFAULT_MAXPDU);
      networkMaxPDU = DEFAULT_MAXPDU;
    }

    if (networkDisableNewVRs)
    {
      dcmEnableUnknownVRGeneration.set(OFFalse);
      dcmEnableUnlimitedTextVRGeneration.set(OFFalse);
      dcmEnableOtherFloatStringVRGeneration.set(OFFalse);
      dcmEnableOtherDoubleStringVRGeneration.set(OFFalse);
    }

    OFOStringStream verboseParameters;

    OFBool comma=OFFalse;
    verboseParameters << "Network parameters:" << OFendl
         << "  port            : " << networkPort << OFendl
         << "  aetitle         : " << networkAETitle << OFendl
         << "  max pdu         : " << networkMaxPDU << OFendl
         << "  options         : ";
    if (networkImplicitVROnly)
    {
      if (comma) verboseParameters << ", "; else comma=OFTrue;
      verboseParameters << "implicit xfer syntax only";
    }
    if (networkBitPreserving)
    {
      if (comma) verboseParameters << ", "; else comma=OFTrue;
      verboseParameters << "bit-preserving receive mode";
    }
    if (networkDisableNewVRs)
    {
      if (comma) verboseParameters << ", "; else comma=OFTrue;
      verboseParameters << "disable post-1993 VRs";
    }
    if (!comma) verboseParameters << "none";
    verboseParameters << OFendl;
    verboseParameters << "  TLS             : ";
    if (useTLS) verboseParameters << "enabled" << OFendl; else verboseParameters << "disabled" << OFendl;

#ifdef WITH_OPENSSL
    if (useTLS)
    {
      verboseParameters << "  TLS certificate : " << tlsCertificateFile << OFendl
           << "  TLS key file    : " << tlsPrivateKeyFile << OFendl
           << "  TLS DH params   : " << tlsDHParametersFile << OFendl
           << "  TLS PRNG seed   : " << tlsRandomSeedFile << OFendl
           << "  TLS CA directory: " << tlsCACertificateFolder << OFendl
           << "  TLS ciphersuites: " << tlsCiphersuites << OFendl
           << "  TLS key format  : ";
      if (keyFileFormat == SSL_FILETYPE_PEM) verboseParameters << "PEM" << OFendl; else verboseParameters << "DER" << OFendl;
      verboseParameters << "  TLS cert verify : ";
      switch (tlsCertVerification)
      {
          case DCV_checkCertificate:
            verboseParameters << "verify" << OFendl;
            break;
          case DCV_ignoreCertificate:
            verboseParameters << "ignore" << OFendl;
            break;
          default:
            verboseParameters << "require" << OFendl;
            break;
      }
    }
#endif

    verboseParameters << OFStringStream_ends;
    OFSTRINGSTREAM_GETSTR(verboseParameters, verboseParametersString)
    OFLOG_INFO(dcmpsrcvLogger, verboseParametersString);

    /* check if we can get access to the database */
    const char *dbfolder = dvi.getDatabaseFolder();

    OFLOG_INFO(dcmpsrcvLogger, "Using database in directory '" << dbfolder << "'");

    OFCondition cond2 = EC_Normal;
    DcmQueryRetrieveIndexDatabaseHandle *dbhandle = new DcmQueryRetrieveIndexDatabaseHandle(dbfolder, PSTAT_MAXSTUDYCOUNT, PSTAT_STUDYSIZE, cond2);
    delete dbhandle;

    if (cond2.bad())
    {
      OFLOG_FATAL(dcmpsrcvLogger, "Unable to access database '" << dbfolder << "'");
      return 1;
    }

    T_ASC_Network *net = NULL; /* the DICOM network and listen port */
    T_ASC_Association *assoc = NULL; /* the DICOM association */
    OFBool finished1 = OFFalse;
    OFBool finished2 = OFFalse;
    int connected = 0;
    OFCondition cond = EC_Normal;

#ifdef WITH_OPENSSL

    DcmTLSTransportLayer *tLayer = NULL;
    if (useTLS)
    {
      tLayer = new DcmTLSTransportLayer(DICOM_APPLICATION_ACCEPTOR, tlsRandomSeedFile.c_str());
      if (tLayer == NULL)
      {
        OFLOG_FATAL(dcmpsrcvLogger, "unable to create TLS transport layer");
        return 1;
      }

      if (tlsCACertificateFolder && (TCS_ok != tLayer->addTrustedCertificateDir(tlsCACertificateFolder, keyFileFormat)))
      {
        OFLOG_WARN(dcmpsrcvLogger, "unable to load certificates from directory '" << tlsCACertificateFolder << "', ignoring");
      }
      if ((tlsDHParametersFile.size() > 0) && ! (tLayer->setTempDHParameters(tlsDHParametersFile.c_str())))
      {
        OFLOG_WARN(dcmpsrcvLogger, "unable to load temporary DH parameter file '" << tlsDHParametersFile << "', ignoring");
      }
      tLayer->setPrivateKeyPasswd(tlsPrivateKeyPassword); // never prompt on console

      if (TCS_ok != tLayer->setPrivateKeyFile(tlsPrivateKeyFile.c_str(), keyFileFormat))
      {
        OFLOG_FATAL(dcmpsrcvLogger, "unable to load private TLS key from '" << tlsPrivateKeyFile<< "'");
        return 1;
      }
      if (TCS_ok != tLayer->setCertificateFile(tlsCertificateFile.c_str(), keyFileFormat))
      {
        OFLOG_FATAL(dcmpsrcvLogger, "unable to load certificate from '" << tlsCertificateFile << "'");
        return 1;
      }
      if (! tLayer->checkPrivateKeyMatchesCertificate())
      {
        OFLOG_FATAL(dcmpsrcvLogger, "private key '" << tlsPrivateKeyFile << "' and certificate '" << tlsCertificateFile << "' do not match");
        return 1;
      }
      if (TCS_ok != tLayer->setCipherSuites(tlsCiphersuites.c_str()))
      {
        OFLOG_FATAL(dcmpsrcvLogger, "unable to set selected cipher suites");
        return 1;
      }

      tLayer->setCertificateVerification(tlsCertVerification);

    }

#endif

    while (!finished1)
    {
      /* open listen socket */
      cond = ASC_initializeNetwork(NET_ACCEPTOR, networkPort, 30, &net);
      if (errorCond(cond, "Error initialising network:"))
      {
        return 1;
      }

#ifdef WITH_OPENSSL
      if (tLayer)
      {
        cond = ASC_setTransportLayer(net, tLayer, 0);
        if (cond.bad())
        {
            OFString temp_str;
            OFLOG_FATAL(dcmpsrcvLogger, DimseCondition::dump(temp_str, cond));
            return 1;
        }
      }
#endif

#if defined(HAVE_SETUID) && defined(HAVE_GETUID)
      /* return to normal uid so that we can't do too much damage in case
       * things go very wrong.   Only relevant if the program is setuid root,
       * and run by another user.  Running as root user may be
       * potentially disasterous if this program screws up badly.
       */
      if ((setuid(getuid()) == -1) && (errno == EAGAIN))
      {
          OFLOG_FATAL(dcmpsrcvLogger, "setuid() failed, maximum number of processes/threads for uid already running.");
          return 1;
      }
#endif

#ifdef HAVE_FORK
      int timeout=1;
#else
      int timeout=1000;
#endif
      while (!finished2)
      {
        /* now we connect to the IPC server and request an application ID */
        if (messageClient) // on Unix, re-initialize for each connect which is later inherited by the forked child
        {
          delete messageClient;
          messageClient = NULL;
        }
        if (messagePort > 0)
        {
          messageClient = new DVPSIPCClient(DVPSIPCMessage::clientStoreSCP, verboseParametersString, messagePort, keepMessagePortOpen);
          if (! messageClient->isServerActive())
          {
            OFLOG_WARN(dcmpsrcvLogger, "no IPC message server found at port " << messagePort << ", disabling IPC");
          }
        }
        connected = 0;
        while (!connected)
        {
           connected = ASC_associationWaiting(net, timeout);
           if (!connected) cleanChildren();
        }
        switch (negotiateAssociation(net, &assoc, networkAETitle, networkMaxPDU, networkImplicitVROnly, useTLS))
        {
          case assoc_error:
            // association has already been deleted, we just wait for the next client to connect.
            break;
          case assoc_terminate:
            finished2=OFTrue;
            finished1=OFTrue;
            notifyTermination = OFFalse; // IPC server will probably already be down
            cond = ASC_dropNetwork(&net);
            if (errorCond(cond, "Error dropping network:")) return 1;
            break;
          case assoc_success:
#ifdef HAVE_FORK
            // Unix version - call fork()
            int pid;
            pid = (int)(fork());
            if (pid < 0)
            {
              char buf[256];
              OFLOG_ERROR(dcmpsrcvLogger, "Cannot create association sub-process: " << OFStandard::strerror(errno, buf, sizeof(buf)));
              refuseAssociation(assoc, ref_CannotFork);

              if (messageClient)
              {
                // notify about rejected association
                OFOStringStream out;
                OFString temp_str;
                out << "DIMSE Association Rejected:" << OFendl
                    << "  reason: cannot create association sub-process: " << OFStandard::strerror(errno, buf, sizeof(buf)) << OFendl
                    << "  calling presentation address: " << assoc->params->DULparams.callingPresentationAddress << OFendl
                    << "  calling AE title: " << assoc->params->DULparams.callingAPTitle << OFendl
                    << "  called AE title: " << assoc->params->DULparams.calledAPTitle << OFendl;
                out << ASC_dumpConnectionParameters(temp_str, assoc) << OFendl;
                out << OFStringStream_ends;
                OFSTRINGSTREAM_GETSTR(out, theString)
                if (useTLS)
                  messageClient->notifyReceivedEncryptedDICOMConnection(DVPSIPCMessage::statusError, theString);
                  else messageClient->notifyReceivedUnencryptedDICOMConnection(DVPSIPCMessage::statusError, theString);
                OFSTRINGSTREAM_FREESTR(theString)
              }
              dropAssociation(&assoc);
            } else if (pid > 0)
            {
              /* parent process */
              assoc = NULL;
            } else {
              /* child process */

#ifdef WITH_OPENSSL
              // a generated UID contains the process ID and current time.
              // Adding it to the PRNG seed guarantees that we have different seeds for
              // different child processes.
              char randomUID[65];
              dcmGenerateUniqueIdentifier(randomUID);
              if (tLayer) tLayer->addPRNGseed(randomUID, strlen(randomUID));
#endif
              handleClient(&assoc, dbfolder, networkBitPreserving, useTLS, opt_correctUIDPadding);
              finished2=OFTrue;
              finished1=OFTrue;
            }
#else
            // Windows version - call CreateProcess()
            finished2=OFTrue;
            cond = ASC_dropNetwork(&net);
            if (errorCond(cond, "Error dropping network:"))
            {
              if (messageClient)
              {
                messageClient->notifyApplicationTerminates(DVPSIPCMessage::statusError);
                delete messageClient;
              }
              return 1;
            }

            // initialize startup info
            const char *receiver_application = dvi.getReceiverName();
            PROCESS_INFORMATION procinfo;
            STARTUPINFO sinfo;
            OFBitmanipTemplate<char>::zeroMem((char *)&sinfo, sizeof(sinfo));
            sinfo.cb = sizeof(sinfo);
            char commandline[4096];
            sprintf(commandline, "%s %s %s", receiver_application, opt_cfgName, opt_cfgID);
#ifdef DEBUG
            if (CreateProcess(NULL, commandline, NULL, NULL, 0, 0, NULL, NULL, &sinfo, &procinfo))
#else
            if (CreateProcess(NULL, commandline, NULL, NULL, 0, DETACHED_PROCESS, NULL, NULL, &sinfo, &procinfo))
#endif
            {
#ifdef WITH_OPENSSL
              // a generated UID contains the process ID and current time.
              // Adding it to the PRNG seed guarantees that we have different seeds for
              // different child processes.
              char randomUID[65];
              dcmGenerateUniqueIdentifier(randomUID);
              if (tLayer) tLayer->addPRNGseed(randomUID, strlen(randomUID));
#endif
              handleClient(&assoc, dbfolder, networkBitPreserving, useTLS, opt_correctUIDPadding);
              finished1=OFTrue;
            } else {
              OFLOG_ERROR(dcmpsrcvLogger, "Cannot execute command line: " << commandline);
              refuseAssociation(assoc, ref_CannotFork);

              if (messageClient)
              {
                // notify about rejected association
                OFOStringStream out;
                out << "DIMSE Association Rejected:" << OFendl
                    << "  reason: cannot execute command line: " << commandline << OFendl
                    << "  calling presentation address: " << assoc->params->DULparams.callingPresentationAddress << OFendl
                    << "  calling AE title: " << assoc->params->DULparams.callingAPTitle << OFendl
                    << "  called AE title: " << assoc->params->DULparams.calledAPTitle << OFendl;
                ASC_dumpConnectionParameters(assoc, out);
                out << OFStringStream_ends;
                OFSTRINGSTREAM_GETSTR(out, theString)
                if (useTLS)
                  messageClient->notifyReceivedEncryptedDICOMConnection(DVPSIPCMessage::statusError, theString);
                  else messageClient->notifyReceivedUnencryptedDICOMConnection(DVPSIPCMessage::statusError, theString);
                OFSTRINGSTREAM_FREESTR(theString)
              }

              dropAssociation(&assoc);
            }
#endif
            break;
        }
      } // finished2
    } // finished1
    cleanChildren();

    // tell the IPC server that we're going to terminate.
    // We need to do this before we shutdown WinSock.
    if (messageClient && notifyTermination)
    {
      messageClient->notifyApplicationTerminates(DVPSIPCMessage::statusOK);
      delete messageClient;
    }

#ifdef HAVE_WINSOCK_H
    WSACleanup();
#endif

#ifdef WITH_OPENSSL
    if (tLayer)
    {
      if (tLayer->canWriteRandomSeed())
      {
        if (!tLayer->writeRandomSeed(tlsRandomSeedFile.c_str()))
        {
          OFLOG_WARN(dcmpsrcvLogger, "cannot write back random seed file '" << tlsRandomSeedFile << "', ignoring");
        }
      } else {
        OFLOG_WARN(dcmpsrcvLogger, "cannot write back random seed, ignoring");
      }
    }
    delete tLayer;
#endif

    OFSTRINGSTREAM_FREESTR(verboseParametersString)

#ifdef DEBUG
    dcmDataDict.clear();  /* useful for debugging with dmalloc */
#endif

    return 0;
}