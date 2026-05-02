main(int argc, char *argv[])
{
    T_ASC_Parameters *params = NULL;
    const char *opt_peer;
    OFCmdUnsignedInt opt_port = 104;
    DIC_NODENAME localHost;
    DIC_NODENAME peerHost;
    T_ASC_Association *assoc = NULL;
    const char *opt_peerTitle = PEERAPPLICATIONTITLE;
    const char *opt_ourTitle = APPLICATIONTITLE;
    OFList<OFString> fileNameList;

#ifdef HAVE_GUSI_H
    /* needed for Macintosh */
    GUSISetup(GUSIwithSIOUXSockets);
    GUSISetup(GUSIwithInternetSockets);
#endif

#ifdef HAVE_WINSOCK_H
    WSAData winSockData;
    /* we need at least version 1.1 */
    WORD winSockVersionNeeded = MAKEWORD( 1, 1 );
    WSAStartup(winSockVersionNeeded, &winSockData);
#endif

  char tempstr[20];
  OFString temp_str;
  OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION , "DICOM retrieve (C-MOVE) SCU", rcsid);
  OFCommandLine cmd;

  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam("peer", "hostname of DICOM peer");
  cmd.addParam("port", "tcp/ip port number of peer");
  cmd.addParam("dcmfile-in", "DICOM query file(s)", OFCmdParam::PM_MultiOptional);

  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
   cmd.addOption("--help",                   "-h",      "print this help text and exit", OFCommandLine::AF_Exclusive);
   cmd.addOption("--version",                           "print version information and exit", OFCommandLine::AF_Exclusive);
   OFLog::addOptions(cmd);

  cmd.addGroup("network options:");
    cmd.addSubGroup("override matching keys:");
      cmd.addOption("--key",                 "-k",   1, "[k]ey: gggg,eeee=\"str\" or dict. name=\"str\"",
                                                        "override matching key");
    cmd.addSubGroup("query information model:");
      cmd.addOption("--patient",             "-P",      "use patient root information model (default)");
      cmd.addOption("--study",               "-S",      "use study root information model");
      cmd.addOption("--psonly",              "-O",      "use patient/study only information model");
    cmd.addSubGroup("application entity titles:");
      OFString opt1 = "set my calling AE title (default: ";
      opt1 += APPLICATIONTITLE;
      opt1 += ")";
      cmd.addOption("--aetitle",             "-aet", 1, "[a]etitle: string", opt1.c_str());
      OFString opt2 = "set called AE title of peer (default: ";
      opt2 += PEERAPPLICATIONTITLE;
      opt2 += ")";
      cmd.addOption("--call",                "-aec", 1, "[a]etitle: string", opt2.c_str());
      OFString opt5 = "set move destinat. AE title (default: ";
      opt5 += APPLICATIONTITLE;
      opt5 += ")";
      cmd.addOption("--move",                "-aem", 1, "[a]etitle: string", opt5.c_str());
    cmd.addSubGroup("preferred network transfer syntaxes (incoming associations):");
      cmd.addOption("--prefer-uncompr",      "+x=",     "prefer explicit VR local byte order (default)");
      cmd.addOption("--prefer-little",       "+xe",     "prefer explicit VR little endian TS");
      cmd.addOption("--prefer-big",          "+xb",     "prefer explicit VR big endian TS");
      cmd.addOption("--prefer-lossless",     "+xs",     "prefer default JPEG lossless TS");
      cmd.addOption("--prefer-jpeg8",        "+xy",     "prefer default JPEG lossy TS for 8 bit data");
      cmd.addOption("--prefer-jpeg12",       "+xx",     "prefer default JPEG lossy TS for 12 bit data");
      cmd.addOption("--prefer-j2k-lossless", "+xv",     "prefer JPEG 2000 lossless TS");
      cmd.addOption("--prefer-j2k-lossy",    "+xw",     "prefer JPEG 2000 lossy TS");
      cmd.addOption("--prefer-jls-lossless", "+xt",     "prefer JPEG-LS lossless TS");
      cmd.addOption("--prefer-jls-lossy",    "+xu",     "prefer JPEG-LS lossy TS");
      cmd.addOption("--prefer-mpeg2",        "+xm",     "prefer MPEG2 Main Profile @ Main Level TS");
      cmd.addOption("--prefer-mpeg2-high",   "+xh",     "prefer MPEG2 Main Profile @ High Level TS");
      cmd.addOption("--prefer-mpeg4",        "+xn",     "prefer MPEG4 AVC/H.264 HP / Level 4.1 TS");
      cmd.addOption("--prefer-mpeg4-bd",     "+xl",     "prefer MPEG4 AVC/H.264 BD-compatible TS");
      cmd.addOption("--prefer-rle",          "+xr",     "prefer RLE lossless TS");
#ifdef WITH_ZLIB
      cmd.addOption("--prefer-deflated",     "+xd",     "prefer deflated explicit VR little endian TS");
#endif
      cmd.addOption("--implicit",            "+xi",     "accept implicit VR little endian TS only");
      cmd.addOption("--accept-all",          "+xa",     "accept all supported transfer syntaxes");
    cmd.addSubGroup("proposed transmission transfer syntaxes (outgoing associations):");
      cmd.addOption("--propose-uncompr",     "-x=",     "propose all uncompressed TS, explicit VR\nwith local byte ordering first (default)");
      cmd.addOption("--propose-little",      "-xe",     "propose all uncompressed TS, explicit VR\nlittle endian first");
      cmd.addOption("--propose-big",         "-xb",     "propose all uncompressed TS, explicit VR\nbig endian first");
#ifdef WITH_ZLIB
      cmd.addOption("--propose-deflated",    "-xd",     "propose deflated explicit VR little endian TS\nand all uncompressed transfer syntaxes");
#endif
      cmd.addOption("--propose-implicit",    "-xi",     "propose implicit VR little endian TS only");
#ifdef WITH_TCPWRAPPER
    cmd.addSubGroup("network host access control (tcp wrapper):");
      cmd.addOption("--access-full",         "-ac",     "accept connections from any host (default)");
      cmd.addOption("--access-control",      "+ac",     "enforce host access control rules");
#endif
    cmd.addSubGroup("port for incoming network associations:");
      cmd.addOption("--no-port",                        "no port for incoming associations (default)");
      cmd.addOption("--port",                "+P",   1, "[n]umber: integer",
                                                        "port number for incoming associations");
    cmd.addSubGroup("handling of illegal datasets following 'pending' move responses:");
      cmd.addOption("--pending-ignore",      "-pi",     "assume no dataset present (default)");
      cmd.addOption("--pending-read",        "-pr",     "read and ignore dataset");

    cmd.addSubGroup("other network options:");
      cmd.addOption("--timeout",             "-to",  1, "[s]econds: integer (default: unlimited)", "timeout for connection requests");
      cmd.addOption("--acse-timeout",        "-ta",  1, "[s]econds: integer (default: 30)", "timeout for ACSE messages");
      cmd.addOption("--dimse-timeout",       "-td",  1, "[s]econds: integer (default: unlimited)", "timeout for DIMSE messages");

      OFString opt3 = "set max receive pdu to n bytes (default: ";
      sprintf(tempstr, "%ld", OFstatic_cast(long, ASC_DEFAULTMAXPDU));
      opt3 += tempstr;
      opt3 += ")";
      OFString opt4 = "[n]umber of bytes: integer (";
      sprintf(tempstr, "%ld", OFstatic_cast(long, ASC_MINIMUMPDUSIZE));
      opt4 += tempstr;
      opt4 += "..";
      sprintf(tempstr, "%ld", OFstatic_cast(long, ASC_MAXIMUMPDUSIZE));
      opt4 += tempstr;
      opt4 += ")";
      cmd.addOption("--max-pdu",             "-pdu", 1, opt4.c_str(), opt3.c_str());
      cmd.addOption("--disable-host-lookup", "-dhl",    "disable hostname lookup");
      cmd.addOption("--repeat",                      1, "[n]umber: integer", "repeat n times");
      cmd.addOption("--abort",                          "abort association instead of releasing it");
      cmd.addOption("--ignore",                         "ignore store data, receive but do not store");
      cmd.addOption("--cancel",                      1, "[n]umber: integer",
                                                        "cancel after n responses (default: never)");
      cmd.addOption("--uid-padding",         "-up",     "silently correct space-padded UIDs");
  cmd.addGroup("output options:");
    cmd.addSubGroup("general:");
      cmd.addOption("--output-directory",    "-od",  1, "[d]irectory: string (default: \".\")", "write received objects to existing directory d");
    cmd.addSubGroup("bit preserving mode:");
      cmd.addOption("--normal",              "-B",      "allow implicit format conversions (default)");
      cmd.addOption("--bit-preserving",      "+B",      "write data exactly as read");
    cmd.addSubGroup("output file format:");
      cmd.addOption("--write-file",          "+F",      "write file format (default)");
      cmd.addOption("--write-dataset",       "-F",      "write data set without file meta information");
    cmd.addSubGroup("output transfer syntax (not with --bit-preserving or compressed transmission):");
      cmd.addOption("--write-xfer-same",     "+t=",     "write with same TS as input (default)");
      cmd.addOption("--write-xfer-little",   "+te",     "write with explicit VR little endian TS");
      cmd.addOption("--write-xfer-big",      "+tb",     "write with explicit VR big endian TS");
      cmd.addOption("--write-xfer-implicit", "+ti",     "write with implicit VR little endian TS");
#ifdef WITH_ZLIB
      cmd.addOption("--write-xfer-deflated", "+td",     "write with deflated expl. VR little endian TS");
#endif
    cmd.addSubGroup("post-1993 value representations (not with --bit-preserving):");
      cmd.addOption("--enable-new-vr",       "+u",      "enable support for new VRs (UN/UT) (default)");
      cmd.addOption("--disable-new-vr",      "-u",      "disable support for new VRs, convert to OB");
    cmd.addSubGroup("group length encoding (not with --bit-preserving):");
      cmd.addOption("--group-length-recalc", "+g=",     "recalculate group lengths if present (default)");
      cmd.addOption("--group-length-create", "+g",      "always write with group length elements");
      cmd.addOption("--group-length-remove", "-g",      "always write without group length elements");
    cmd.addSubGroup("length encoding in sequences and items (not with --bit-preserving):");
      cmd.addOption("--length-explicit",     "+e",      "write with explicit lengths (default)");
      cmd.addOption("--length-undefined",    "-e",      "write with undefined lengths");
    cmd.addSubGroup("data set trailing padding (not with --write-dataset or --bit-preserving):");
      cmd.addOption("--padding-off",         "-p",      "no padding (default)");
      cmd.addOption("--padding-create",      "+p",   2, "[f]ile-pad [i]tem-pad: integer",
                                                        "align file on multiple of f bytes\nand items on multiple of i bytes");
#ifdef WITH_ZLIB
    cmd.addSubGroup("deflate compression level (only with -xd or --write-xfer-deflated/same):");
      cmd.addOption("--compression-level",   "+cl",  1, "[l]evel: integer (default: 6)",
                                                        "0=uncompressed, 1=fastest, 9=best compression");
#endif

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
#if !defined(WITH_ZLIB) && !defined(WITH_TCPWRAPPER)
          COUT << " none" << OFendl;
#else
          COUT << OFendl;
#endif
#ifdef WITH_ZLIB
          COUT << "- ZLIB, Version " << zlibVersion() << OFendl;
#endif
#ifdef WITH_TCPWRAPPER
          COUT << "- LIBWRAP" << OFendl;
#endif
          return 0;
        }
      }

      /* command line parameters */

      cmd.getParam(1, opt_peer);
      app.checkParam(cmd.getParamAndCheckMinMax(2, opt_port, 1, 65535));

      OFLog::configureFromCommandLine(cmd, app);

      if (cmd.findOption("--key", 0, OFCommandLine::FOM_First))
      {
        const char *ovKey = NULL;
        do {
          app.checkValue(cmd.getValue(ovKey));
          addOverrideKey(app, ovKey);
        } while (cmd.findOption("--key", 0, OFCommandLine::FOM_Next));
      }
      cmd.beginOptionBlock();
      if (cmd.findOption("--patient")) opt_queryModel = QMPatientRoot;
      if (cmd.findOption("--study")) opt_queryModel = QMStudyRoot;
      if (cmd.findOption("--psonly")) opt_queryModel = QMPatientStudyOnly;
      cmd.endOptionBlock();

      if (cmd.findOption("--aetitle")) app.checkValue(cmd.getValue(opt_ourTitle));
      if (cmd.findOption("--call")) app.checkValue(cmd.getValue(opt_peerTitle));
      if (cmd.findOption("--move")) app.checkValue(cmd.getValue(opt_moveDestination));

      cmd.beginOptionBlock();
      if (cmd.findOption("--prefer-uncompr"))
      {
          opt_acceptAllXfers = OFFalse;
          opt_in_networkTransferSyntax = EXS_Unknown;
      }
      if (cmd.findOption("--prefer-little")) opt_in_networkTransferSyntax = EXS_LittleEndianExplicit;
      if (cmd.findOption("--prefer-big")) opt_in_networkTransferSyntax = EXS_BigEndianExplicit;
      if (cmd.findOption("--prefer-lossless")) opt_in_networkTransferSyntax = EXS_JPEGProcess14SV1;
      if (cmd.findOption("--prefer-jpeg8")) opt_in_networkTransferSyntax = EXS_JPEGProcess1;
      if (cmd.findOption("--prefer-jpeg12")) opt_in_networkTransferSyntax = EXS_JPEGProcess2_4;
      if (cmd.findOption("--prefer-j2k-lossless")) opt_in_networkTransferSyntax = EXS_JPEG2000LosslessOnly;
      if (cmd.findOption("--prefer-j2k-lossy")) opt_in_networkTransferSyntax = EXS_JPEG2000;
      if (cmd.findOption("--prefer-jls-lossless")) opt_in_networkTransferSyntax = EXS_JPEGLSLossless;
      if (cmd.findOption("--prefer-jls-lossy")) opt_in_networkTransferSyntax = EXS_JPEGLSLossy;
      if (cmd.findOption("--prefer-mpeg2")) opt_in_networkTransferSyntax = EXS_MPEG2MainProfileAtMainLevel;
      if (cmd.findOption("--prefer-mpeg2-high")) opt_in_networkTransferSyntax = EXS_MPEG2MainProfileAtHighLevel;
      if (cmd.findOption("--prefer-mpeg4")) opt_in_networkTransferSyntax = EXS_MPEG4HighProfileLevel4_1;
      if (cmd.findOption("--prefer-mpeg4-bd")) opt_in_networkTransferSyntax = EXS_MPEG4BDcompatibleHighProfileLevel4_1;
      if (cmd.findOption("--prefer-rle")) opt_in_networkTransferSyntax = EXS_RLELossless;
#ifdef WITH_ZLIB
      if (cmd.findOption("--prefer-deflated")) opt_in_networkTransferSyntax = EXS_DeflatedLittleEndianExplicit;
#endif
      if (cmd.findOption("--implicit")) opt_in_networkTransferSyntax = EXS_LittleEndianImplicit;
      if (cmd.findOption("--accept-all"))
      {
          opt_acceptAllXfers = OFTrue;
          opt_in_networkTransferSyntax = EXS_Unknown;
      }
      cmd.endOptionBlock();
      if (opt_in_networkTransferSyntax != EXS_Unknown) opt_acceptAllXfers = OFFalse;

      cmd.beginOptionBlock();
      if (cmd.findOption("--propose-uncompr")) opt_out_networkTransferSyntax = EXS_Unknown;
      if (cmd.findOption("--propose-little")) opt_out_networkTransferSyntax = EXS_LittleEndianExplicit;
      if (cmd.findOption("--propose-big")) opt_out_networkTransferSyntax = EXS_BigEndianExplicit;
      if (cmd.findOption("--propose-implicit")) opt_out_networkTransferSyntax = EXS_LittleEndianImplicit;
#ifdef WITH_ZLIB
      if (cmd.findOption("--propose-deflated")) opt_out_networkTransferSyntax = EXS_DeflatedLittleEndianExplicit;
#endif
      cmd.endOptionBlock();

#ifdef WITH_TCPWRAPPER
      cmd.beginOptionBlock();
      if (cmd.findOption("--access-full")) dcmTCPWrapperDaemonName.set(NULL);
      if (cmd.findOption("--access-control")) dcmTCPWrapperDaemonName.set(OFFIS_CONSOLE_APPLICATION);
      cmd.endOptionBlock();
#endif

      if (cmd.findOption("--timeout"))
      {
        OFCmdSignedInt opt_timeout = 0;
        app.checkValue(cmd.getValueAndCheckMin(opt_timeout, 1));
        dcmConnectionTimeout.set(OFstatic_cast(Sint32, opt_timeout));
      }

      if (cmd.findOption("--acse-timeout"))
      {
        OFCmdSignedInt opt_timeout = 0;
        app.checkValue(cmd.getValueAndCheckMin(opt_timeout, 1));
        opt_acse_timeout = OFstatic_cast(int, opt_timeout);
      }

      if (cmd.findOption("--dimse-timeout"))
      {
        OFCmdSignedInt opt_timeout = 0;
        app.checkValue(cmd.getValueAndCheckMin(opt_timeout, 1));
        opt_dimse_timeout = OFstatic_cast(int, opt_timeout);
        opt_blockMode = DIMSE_NONBLOCKING;
      }

      cmd.beginOptionBlock();
      if (cmd.findOption("--port"))    app.checkValue(cmd.getValueAndCheckMinMax(opt_retrievePort, 1, 65535));
      if (cmd.findOption("--no-port")) { /* do nothing */ }
      cmd.endOptionBlock();

      cmd.beginOptionBlock();
      if (cmd.findOption("--pending-ignore")) opt_ignorePendingDatasets = OFTrue;
      if (cmd.findOption("--pending-read")) opt_ignorePendingDatasets = OFFalse;
      cmd.endOptionBlock();

      if (cmd.findOption("--max-pdu")) app.checkValue(cmd.getValueAndCheckMinMax(opt_maxPDU, ASC_MINIMUMPDUSIZE, ASC_MAXIMUMPDUSIZE));
      if (cmd.findOption("--disable-host-lookup")) dcmDisableGethostbyaddr.set(OFTrue);
      if (cmd.findOption("--repeat"))  app.checkValue(cmd.getValueAndCheckMin(opt_repeatCount, 1));
      if (cmd.findOption("--abort"))   opt_abortAssociation = OFTrue;
      if (cmd.findOption("--ignore"))  opt_ignore = OFTrue;
      if (cmd.findOption("--cancel"))  app.checkValue(cmd.getValueAndCheckMin(opt_cancelAfterNResponses, 0));
      if (cmd.findOption("--uid-padding")) opt_correctUIDPadding = OFTrue;

      if (cmd.findOption("--output-directory")) app.checkValue(cmd.getValue(opt_outputDirectory));

      cmd.beginOptionBlock();
      if (cmd.findOption("--normal")) opt_bitPreserving = OFFalse;
      if (cmd.findOption("--bit-preserving")) opt_bitPreserving = OFTrue;
      cmd.endOptionBlock();

      cmd.beginOptionBlock();
      if (cmd.findOption("--write-file")) opt_useMetaheader = OFTrue;
      if (cmd.findOption("--write-dataset")) opt_useMetaheader = OFFalse;
      cmd.endOptionBlock();

      cmd.beginOptionBlock();
      if (cmd.findOption("--write-xfer-same")) opt_writeTransferSyntax = EXS_Unknown;
      if (cmd.findOption("--write-xfer-little"))
      {
        app.checkConflict("--write-xfer-little", "--accept-all", opt_acceptAllXfers);
        app.checkConflict("--write-xfer-little", "--bit-preserving", opt_bitPreserving);
        app.checkConflict("--write-xfer-little", "--prefer-lossless", opt_in_networkTransferSyntax == EXS_JPEGProcess14SV1);
        app.checkConflict("--write-xfer-little", "--prefer-jpeg8", opt_in_networkTransferSyntax == EXS_JPEGProcess1);
        app.checkConflict("--write-xfer-little", "--prefer-jpeg12", opt_in_networkTransferSyntax == EXS_JPEGProcess2_4);
        app.checkConflict("--write-xfer-little", "--prefer-j2k-lossless", opt_in_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
        app.checkConflict("--write-xfer-little", "--prefer-j2k-lossy", opt_in_networkTransferSyntax == EXS_JPEG2000);
        app.checkConflict("--write-xfer-little", "--prefer-jls-lossless", opt_in_networkTransferSyntax == EXS_JPEGLSLossless);
        app.checkConflict("--write-xfer-little", "--prefer-jls-lossy", opt_in_networkTransferSyntax == EXS_JPEGLSLossy);
        app.checkConflict("--write-xfer-little", "--prefer-mpeg2", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
        app.checkConflict("--write-xfer-little", "--prefer-mpeg2-high", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
        app.checkConflict("--write-xfer-little", "--prefer-mpeg4", opt_in_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
        app.checkConflict("--write-xfer-little", "--prefer-mpeg4-bd", opt_in_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
        app.checkConflict("--write-xfer-little", "--prefer-rle", opt_in_networkTransferSyntax == EXS_RLELossless);
        // we don't have to check a conflict for --prefer-deflated because we can always convert that to uncompressed.
        opt_writeTransferSyntax = EXS_LittleEndianExplicit;
      }
      if (cmd.findOption("--write-xfer-big"))
      {
        app.checkConflict("--write-xfer-big", "--accept-all", opt_acceptAllXfers);
        app.checkConflict("--write-xfer-big", "--bit-preserving", opt_bitPreserving);
        app.checkConflict("--write-xfer-big", "--prefer-lossless", opt_in_networkTransferSyntax == EXS_JPEGProcess14SV1);
        app.checkConflict("--write-xfer-big", "--prefer-jpeg8", opt_in_networkTransferSyntax == EXS_JPEGProcess1);
        app.checkConflict("--write-xfer-big", "--prefer-jpeg12", opt_in_networkTransferSyntax == EXS_JPEGProcess2_4);
        app.checkConflict("--write-xfer-big", "--prefer-j2k-lossy", opt_in_networkTransferSyntax == EXS_JPEG2000);
        app.checkConflict("--write-xfer-big", "--prefer-j2k-lossless", opt_in_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
        app.checkConflict("--write-xfer-big", "--prefer-jls-lossless", opt_in_networkTransferSyntax == EXS_JPEGLSLossless);
        app.checkConflict("--write-xfer-big", "--prefer-jls-lossy", opt_in_networkTransferSyntax == EXS_JPEGLSLossy);
        app.checkConflict("--write-xfer-big", "--prefer-mpeg2", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
        app.checkConflict("--write-xfer-big", "--prefer-mpeg2-high", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
        app.checkConflict("--write-xfer-big", "--prefer-mpeg4", opt_in_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
        app.checkConflict("--write-xfer-big", "--prefer-mpeg4-bd", opt_in_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
        app.checkConflict("--write-xfer-big", "--prefer-rle", opt_in_networkTransferSyntax == EXS_RLELossless);
        // we don't have to check a conflict for --prefer-deflated because we can always convert that to uncompressed.
        opt_writeTransferSyntax = EXS_BigEndianExplicit;
      }
      if (cmd.findOption("--write-xfer-implicit"))
      {
        app.checkConflict("--write-xfer-implicit", "--accept-all", opt_acceptAllXfers);
        app.checkConflict("--write-xfer-implicit", "--bit-preserving", opt_bitPreserving);
        app.checkConflict("--write-xfer-implicit", "--prefer-lossless", opt_in_networkTransferSyntax == EXS_JPEGProcess14SV1);
        app.checkConflict("--write-xfer-implicit", "--prefer-jpeg8", opt_in_networkTransferSyntax == EXS_JPEGProcess1);
        app.checkConflict("--write-xfer-implicit", "--prefer-jpeg12", opt_in_networkTransferSyntax == EXS_JPEGProcess2_4);
        app.checkConflict("--write-xfer-implicit", "--prefer-j2k-lossy", opt_in_networkTransferSyntax == EXS_JPEG2000);
        app.checkConflict("--write-xfer-implicit", "--prefer-j2k-lossless", opt_in_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
        app.checkConflict("--write-xfer-implicit", "--prefer-jls-lossless", opt_in_networkTransferSyntax == EXS_JPEGLSLossless);
        app.checkConflict("--write-xfer-implicit", "--prefer-jls-lossy", opt_in_networkTransferSyntax == EXS_JPEGLSLossy);
        app.checkConflict("--write-xfer-implicit", "--prefer-mpeg2", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
        app.checkConflict("--write-xfer-implicit", "--prefer-mpeg2-high", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
        app.checkConflict("--write-xfer-implicit", "--prefer-mpeg4", opt_in_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
        app.checkConflict("--write-xfer-implicit", "--prefer-mpeg4-bd", opt_in_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
        app.checkConflict("--write-xfer-implicit", "--prefer-rle", opt_in_networkTransferSyntax == EXS_RLELossless);
        // we don't have to check a conflict for --prefer-deflated because we can always convert that to uncompressed.
        opt_writeTransferSyntax = EXS_LittleEndianImplicit;
      }
#ifdef WITH_ZLIB
      if (cmd.findOption("--write-xfer-deflated"))
      {
        app.checkConflict("--write-xfer-deflated", "--accept-all", opt_acceptAllXfers);
        app.checkConflict("--write-xfer-deflated", "--bit-preserving", opt_bitPreserving);
        app.checkConflict("--write-xfer-deflated", "--prefer-lossless", opt_in_networkTransferSyntax == EXS_JPEGProcess14SV1);
        app.checkConflict("--write-xfer-deflated", "--prefer-jpeg8", opt_in_networkTransferSyntax == EXS_JPEGProcess1);
        app.checkConflict("--write-xfer-deflated", "--prefer-jpeg12", opt_in_networkTransferSyntax == EXS_JPEGProcess2_4);
        app.checkConflict("--write-xfer-deflated", "--prefer-j2k-lossless", opt_in_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
        app.checkConflict("--write-xfer-deflated", "--prefer-j2k-lossy", opt_in_networkTransferSyntax == EXS_JPEG2000);
        app.checkConflict("--write-xfer-deflated", "--prefer-jls-lossless", opt_in_networkTransferSyntax == EXS_JPEGLSLossless);
        app.checkConflict("--write-xfer-deflated", "--prefer-jls-lossy", opt_in_networkTransferSyntax == EXS_JPEGLSLossy);
        app.checkConflict("--write-xfer-deflated", "--prefer-mpeg2", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
        app.checkConflict("--write-xfer-deflated", "--prefer-mpeg2-high", opt_in_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
        app.checkConflict("--write-xfer-deflated", "--prefer-mpeg4", opt_in_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
        app.checkConflict("--write-xfer-deflated", "--prefer-mpeg4-bd", opt_in_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
        app.checkConflict("--write-xfer-deflated", "--prefer-rle", opt_in_networkTransferSyntax == EXS_RLELossless);
        opt_writeTransferSyntax = EXS_DeflatedLittleEndianExplicit;
      }
#endif
      cmd.endOptionBlock();

      cmd.beginOptionBlock();
      if (cmd.findOption("--enable-new-vr"))
      {
        app.checkConflict("--enable-new-vr", "--bit-preserving", opt_bitPreserving);
        dcmEnableUnknownVRGeneration.set(OFTrue);
        dcmEnableUnlimitedTextVRGeneration.set(OFTrue);
        dcmEnableOtherFloatStringVRGeneration.set(OFTrue);
        dcmEnableOtherDoubleStringVRGeneration.set(OFTrue);
      }
      if (cmd.findOption("--disable-new-vr"))
      {
        app.checkConflict("--disable-new-vr", "--bit-preserving", opt_bitPreserving);
        dcmEnableUnknownVRGeneration.set(OFFalse);
        dcmEnableUnlimitedTextVRGeneration.set(OFFalse);
        dcmEnableOtherFloatStringVRGeneration.set(OFFalse);
        dcmEnableOtherDoubleStringVRGeneration.set(OFFalse);
      }
      cmd.endOptionBlock();

      cmd.beginOptionBlock();
      if (cmd.findOption("--group-length-recalc"))
      {
        app.checkConflict("--group-length-recalc", "--bit-preserving", opt_bitPreserving);
        opt_groupLength = EGL_recalcGL;
      }
      if (cmd.findOption("--group-length-create"))
      {
        app.checkConflict("--group-length-create", "--bit-preserving", opt_bitPreserving);
        opt_groupLength = EGL_withGL;
      }
      if (cmd.findOption("--group-length-remove"))
      {
        app.checkConflict("--group-length-remove", "--bit-preserving", opt_bitPreserving);
        opt_groupLength = EGL_withoutGL;
      }
      cmd.endOptionBlock();

      cmd.beginOptionBlock();
      if (cmd.findOption("--length-explicit"))
      {
        app.checkConflict("--length-explicit", "--bit-preserving", opt_bitPreserving);
        opt_sequenceType = EET_ExplicitLength;
      }
      if (cmd.findOption("--length-undefined"))
      {
        app.checkConflict("--length-undefined", "--bit-preserving", opt_bitPreserving);
        opt_sequenceType = EET_UndefinedLength;
      }
      cmd.endOptionBlock();

      cmd.beginOptionBlock();
      if (cmd.findOption("--padding-off")) opt_paddingType = EPD_withoutPadding;
      if (cmd.findOption("--padding-create"))
      {
        app.checkConflict("--padding-create", "--write-dataset", !opt_useMetaheader);
        app.checkConflict("--padding-create", "--bit-preserving", opt_bitPreserving);
        app.checkValue(cmd.getValueAndCheckMin(opt_filepad, 0));
        app.checkValue(cmd.getValueAndCheckMin(opt_itempad, 0));
        opt_paddingType = EPD_withPadding;
      }
      cmd.endOptionBlock();

#ifdef WITH_ZLIB
      if (cmd.findOption("--compression-level"))
      {
        app.checkDependence("--compression-level", "--propose-deflated, --write-xfer-deflated or --write-xfer-same",
          (opt_out_networkTransferSyntax == EXS_DeflatedLittleEndianExplicit) ||
          (opt_writeTransferSyntax == EXS_DeflatedLittleEndianExplicit) ||
          (opt_writeTransferSyntax == EXS_Unknown));
        app.checkValue(cmd.getValueAndCheckMinMax(opt_compressionLevel, 0, 9));
        dcmZlibCompressionLevel.set(OFstatic_cast(int, opt_compressionLevel));
      }
#endif

      /* finally parse filenames */
      int paramCount = cmd.getParamCount();
      const char *currentFilename = NULL;
      OFString errormsg;

      for (int i=3; i <= paramCount; i++)
      {
        cmd.getParam(i, currentFilename);
        if (access(currentFilename, R_OK) < 0)
        {
          errormsg = "cannot access file: ";
          errormsg += currentFilename;
          app.printError(errormsg.c_str());
        }
        fileNameList.push_back(currentFilename);
      }

      if ((fileNameList.empty()) && (overrideKeys == NULL))
      {
        app.printError("either query file or override keys (or both) must be specified");
      }
    }

    /* print resource identifier */
    OFLOG_DEBUG(movescuLogger, rcsid << OFendl);

    /* make sure data dictionary is loaded */
    if (!dcmDataDict.isDictionaryLoaded())
    {
        OFLOG_WARN(movescuLogger, "no data dictionary loaded, check environment variable: "
            << DCM_DICT_ENVIRONMENT_VARIABLE);
    }

    /* make sure output directory exists and is writeable */
    if (opt_retrievePort > 0)
    {
        if (!OFStandard::dirExists(opt_outputDirectory))
        {
          OFLOG_FATAL(movescuLogger, "specified output directory does not exist");
          return 1;
        }
        else if (!OFStandard::isWriteable(opt_outputDirectory))
        {
          OFLOG_FATAL(movescuLogger, "specified output directory is not writeable");
          return 1;
        }
    }

#ifndef DISABLE_PORT_PERMISSION_CHECK
#ifdef HAVE_GETEUID
    /* if retrieve port is privileged we must be as well */
    if ((opt_retrievePort > 0) && (opt_retrievePort < 1024)) {
        if (geteuid() != 0)
        {
          OFLOG_FATAL(movescuLogger, "cannot listen on port " << opt_retrievePort << ", insufficient privileges");
          return 1;
        }
    }
#endif
#endif

    /* network for move request and responses */
    T_ASC_NetworkRole role = (opt_retrievePort > 0) ? NET_ACCEPTORREQUESTOR : NET_REQUESTOR;
    OFCondition cond = ASC_initializeNetwork(role, OFstatic_cast(int, opt_retrievePort), opt_acse_timeout, &net);
    if (cond.bad())
    {
        OFLOG_FATAL(movescuLogger, "cannot create network: " << DimseCondition::dump(temp_str, cond));
        return 1;
    }

#ifdef HAVE_GETUID
    /* return to normal uid so that we can't do too much damage in case
     * things go very wrong.   Only does someting if the program is setuid
     * root, and run by another user.  Running as root user may be
     * potentially disasterous if this program screws up badly.
     */
    if ((setuid(getuid()) == -1) && (errno == EAGAIN))
    {
        OFLOG_FATAL(movescuLogger, "setuid() failed, maximum number of processes/threads for uid already running.");
        return 1;
    }
#endif

    /* set up main association */
    cond = ASC_createAssociationParameters(&params, opt_maxPDU);
    if (cond.bad()) {
        OFLOG_FATAL(movescuLogger, DimseCondition::dump(temp_str, cond));
        exit(1);
    }
    ASC_setAPTitles(params, opt_ourTitle, opt_peerTitle, NULL);

    gethostname(localHost, sizeof(localHost) - 1);
    sprintf(peerHost, "%s:%d", opt_peer, OFstatic_cast(int, opt_port));
    ASC_setPresentationAddresses(params, localHost, peerHost);

    /*
     * We also add a presentation context for the corresponding
     * find sop class.
     */
    cond = addPresentationContext(params, 1,
        querySyntax[opt_queryModel].findSyntax);

    cond = addPresentationContext(params, 3,
        querySyntax[opt_queryModel].moveSyntax);
    if (cond.bad()) {
        OFLOG_FATAL(movescuLogger, DimseCondition::dump(temp_str, cond));
        exit(1);
    }

    OFLOG_DEBUG(movescuLogger, "Request Parameters:" << OFendl << ASC_dumpParameters(temp_str, params, ASC_ASSOC_RQ));

    /* create association */
    OFLOG_INFO(movescuLogger, "Requesting Association");
    cond = ASC_requestAssociation(net, params, &assoc);
    if (cond.bad()) {
        if (cond == DUL_ASSOCIATIONREJECTED) {
            T_ASC_RejectParameters rej;

            ASC_getRejectParameters(params, &rej);
            OFLOG_FATAL(movescuLogger, "Association Rejected:");
            OFLOG_FATAL(movescuLogger, ASC_printRejectParameters(temp_str, &rej));
            exit(1);
        } else {
            OFLOG_FATAL(movescuLogger, "Association Request Failed:");
            OFLOG_FATAL(movescuLogger, DimseCondition::dump(temp_str, cond));
            exit(1);
        }
    }
    /* what has been accepted/refused ? */
    OFLOG_DEBUG(movescuLogger, "Association Parameters Negotiated:" << OFendl << ASC_dumpParameters(temp_str, params, ASC_ASSOC_AC));

    if (ASC_countAcceptedPresentationContexts(params) == 0) {
        OFLOG_FATAL(movescuLogger, "No Acceptable Presentation Contexts");
        exit(1);
    }

    OFLOG_INFO(movescuLogger, "Association Accepted (Max Send PDV: " << assoc->sendPDVLength << ")");

    /* do the real work */
    cond = EC_Normal;
    if (fileNameList.empty())
    {
      /* no files provided on command line */
      cond = cmove(assoc, NULL);
    } else {
      OFListIterator(OFString) iter = fileNameList.begin();
      OFListIterator(OFString) enditer = fileNameList.end();
      while ((iter != enditer) && cond.good())
      {
          cond = cmove(assoc, (*iter).c_str());
          ++iter;
      }
    }

    /* tear down association */
    if (cond == EC_Normal)
    {
        if (opt_abortAssociation) {
            OFLOG_INFO(movescuLogger, "Aborting Association");
            cond = ASC_abortAssociation(assoc);
            if (cond.bad()) {
                OFLOG_FATAL(movescuLogger, "Association Abort Failed: " << DimseCondition::dump(temp_str, cond));
                exit(1);
            }
        } else {
            /* release association */
            OFLOG_INFO(movescuLogger, "Releasing Association");
            cond = ASC_releaseAssociation(assoc);
            if (cond.bad())
            {
                OFLOG_FATAL(movescuLogger, "Association Release Failed:");
                OFLOG_FATAL(movescuLogger, DimseCondition::dump(temp_str, cond));
                exit(1);
            }
        }
    }
    else if (cond == DUL_PEERREQUESTEDRELEASE)
    {
        OFLOG_ERROR(movescuLogger, "Protocol Error: Peer requested release (Aborting)");
        OFLOG_INFO(movescuLogger, "Aborting Association");
        cond = ASC_abortAssociation(assoc);
        if (cond.bad()) {
            OFLOG_FATAL(movescuLogger, "Association Abort Failed: " << DimseCondition::dump(temp_str, cond));
            exit(1);
        }
    }
    else if (cond == DUL_PEERABORTEDASSOCIATION)
    {
        OFLOG_INFO(movescuLogger, "Peer Aborted Association");
    }
    else
    {
        OFLOG_ERROR(movescuLogger, "Move SCU Failed: " << DimseCondition::dump(temp_str, cond));
        OFLOG_INFO(movescuLogger, "Aborting Association");
        cond = ASC_abortAssociation(assoc);
        if (cond.bad()) {
            OFLOG_FATAL(movescuLogger, "Association Abort Failed: " << DimseCondition::dump(temp_str, cond));
            exit(1);
        }
    }

    cond = ASC_destroyAssociation(&assoc);
    if (cond.bad()) {
        OFLOG_FATAL(movescuLogger, DimseCondition::dump(temp_str, cond));
        exit(1);
    }
    cond = ASC_dropNetwork(&net);
    if (cond.bad()) {
        OFLOG_FATAL(movescuLogger, DimseCondition::dump(temp_str, cond));
        exit(1);
    }

#ifdef HAVE_WINSOCK_H
    WSACleanup();
#endif

    return 0;
}