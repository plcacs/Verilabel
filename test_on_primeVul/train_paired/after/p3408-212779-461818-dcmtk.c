int main(int argc, char *argv[])
{
  T_ASC_Network *net;
  DcmAssociationConfiguration asccfg;

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
  OFConsoleApplication app(OFFIS_CONSOLE_APPLICATION, "DICOM storage (C-STORE) SCP", rcsid);
  OFCommandLine cmd;

  cmd.setParamColumn(LONGCOL + SHORTCOL + 4);
  cmd.addParam("port", "tcp/ip port number to listen on", OFCmdParam::PM_Optional);

  cmd.setOptionColumns(LONGCOL, SHORTCOL);
  cmd.addGroup("general options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--help",                     "-h",      "print this help text and exit", OFCommandLine::AF_Exclusive);
    cmd.addOption("--version",                             "print version information and exit", OFCommandLine::AF_Exclusive);
    OFLog::addOptions(cmd);
    cmd.addOption("--verbose-pc",               "+v",      "show presentation contexts in verbose mode");

#if defined(HAVE_FORK) || defined(_WIN32)
  cmd.addGroup("multi-process options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--single-process",                      "single process mode (default)");
    cmd.addOption("--fork",                                "fork child process for each association");
#ifdef _WIN32
    cmd.addOption("--forked-child",                        "process is forked child, internal use only", OFCommandLine::AF_Internal);
#endif
#endif

  cmd.addGroup("network options:");
    cmd.addSubGroup("association negotiation profile from configuration file:");
      cmd.addOption("--config-file",            "-xf",  2, "[f]ilename, [p]rofile: string",
                                                           "use profile p from config file f");
    cmd.addSubGroup("preferred network transfer syntaxes (not with --config-file):");
      cmd.addOption("--prefer-uncompr",         "+x=",     "prefer explicit VR local byte order (default)");
      cmd.addOption("--prefer-little",          "+xe",     "prefer explicit VR little endian TS");
      cmd.addOption("--prefer-big",             "+xb",     "prefer explicit VR big endian TS");
      cmd.addOption("--prefer-lossless",        "+xs",     "prefer default JPEG lossless TS");
      cmd.addOption("--prefer-jpeg8",           "+xy",     "prefer default JPEG lossy TS for 8 bit data");
      cmd.addOption("--prefer-jpeg12",          "+xx",     "prefer default JPEG lossy TS for 12 bit data");
      cmd.addOption("--prefer-j2k-lossless",    "+xv",     "prefer JPEG 2000 lossless TS");
      cmd.addOption("--prefer-j2k-lossy",       "+xw",     "prefer JPEG 2000 lossy TS");
      cmd.addOption("--prefer-jls-lossless",    "+xt",     "prefer JPEG-LS lossless TS");
      cmd.addOption("--prefer-jls-lossy",       "+xu",     "prefer JPEG-LS lossy TS");
      cmd.addOption("--prefer-mpeg2",           "+xm",     "prefer MPEG2 Main Profile @ Main Level TS");
      cmd.addOption("--prefer-mpeg2-high",      "+xh",     "prefer MPEG2 Main Profile @ High Level TS");
      cmd.addOption("--prefer-mpeg4",           "+xn",     "prefer MPEG4 AVC/H.264 HP / Level 4.1 TS");
      cmd.addOption("--prefer-mpeg4-bd",        "+xl",     "prefer MPEG4 AVC/H.264 BD-compatible TS");
      cmd.addOption("--prefer-rle",             "+xr",     "prefer RLE lossless TS");
#ifdef WITH_ZLIB
      cmd.addOption("--prefer-deflated",        "+xd",     "prefer deflated expl. VR little endian TS");
#endif
      cmd.addOption("--implicit",               "+xi",     "accept implicit VR little endian TS only");
      cmd.addOption("--accept-all",             "+xa",     "accept all supported transfer syntaxes");

#ifdef WITH_TCPWRAPPER
    cmd.addSubGroup("network host access control (tcp wrapper):");
      cmd.addOption("--access-full",            "-ac",     "accept connections from any host (default)");
      cmd.addOption("--access-control",         "+ac",     "enforce host access control rules");
#endif

    cmd.addSubGroup("other network options:");
#ifdef INETD_AVAILABLE
      // this option is only offered on Posix platforms
      cmd.addOption("--inetd",                  "-id",     "run from inetd super server (not with --fork)");
#endif

      cmd.addOption("--acse-timeout",           "-ta",  1, "[s]econds: integer (default: 30)", "timeout for ACSE messages");
      cmd.addOption("--dimse-timeout",          "-td",  1, "[s]econds: integer (default: unlimited)", "timeout for DIMSE messages");

      OFString opt1 = "set my AE title (default: ";
      opt1 += APPLICATIONTITLE;
      opt1 += ")";
      cmd.addOption("--aetitle",                "-aet", 1, "[a]etitle: string", opt1.c_str());
      OFString opt3 = "set max receive pdu to n bytes (def.: ";
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
      cmd.addOption("--max-pdu",                "-pdu", 1, opt4.c_str(), opt3.c_str());
      cmd.addOption("--disable-host-lookup",    "-dhl",    "disable hostname lookup");
      cmd.addOption("--refuse",                            "refuse association");
      cmd.addOption("--reject",                            "reject association if no implement. class UID");
      cmd.addOption("--ignore",                            "ignore store data, receive but do not store");
      cmd.addOption("--sleep-after",                    1, "[s]econds: integer",
                                                           "sleep s seconds after store (default: 0)");
      cmd.addOption("--sleep-during",                   1, "[s]econds: integer",
                                                           "sleep s seconds during store (default: 0)");
      cmd.addOption("--abort-after",                       "abort association after receipt of C-STORE-RQ\n(but before sending response)");
      cmd.addOption("--abort-during",                      "abort association during receipt of C-STORE-RQ");
      cmd.addOption("--promiscuous",            "-pm",     "promiscuous mode, accept unknown SOP classes\n(not with --config-file)");
      cmd.addOption("--uid-padding",            "-up",     "silently correct space-padded UIDs");

#ifdef WITH_OPENSSL
  cmd.addGroup("transport layer security (TLS) options:");
    cmd.addSubGroup("transport protocol stack:");
      cmd.addOption("--disable-tls",            "-tls",    "use normal TCP/IP connection (default)");
      cmd.addOption("--enable-tls",             "+tls", 2, "[p]rivate key file, [c]ertificate file: string",
                                                           "use authenticated secure TLS connection");
    cmd.addSubGroup("private key password (only with --enable-tls):");
      cmd.addOption("--std-passwd",             "+ps",     "prompt user to type password on stdin (default)");
      cmd.addOption("--use-passwd",             "+pw",  1, "[p]assword: string",
                                                           "use specified password");
      cmd.addOption("--null-passwd",            "-pw",     "use empty string as password");
    cmd.addSubGroup("key and certificate file format:");
      cmd.addOption("--pem-keys",               "-pem",    "read keys and certificates as PEM file (def.)");
      cmd.addOption("--der-keys",               "-der",    "read keys and certificates as DER file");
    cmd.addSubGroup("certification authority:");
      cmd.addOption("--add-cert-file",          "+cf",  1, "[c]ertificate filename: string",
                                                           "add certificate file to list of certificates", OFCommandLine::AF_NoWarning);
      cmd.addOption("--add-cert-dir",           "+cd",  1, "[c]ertificate directory: string",
                                                           "add certificates in d to list of certificates", OFCommandLine::AF_NoWarning);
    cmd.addSubGroup("ciphersuite:");
      cmd.addOption("--cipher",                 "+cs",  1, "[c]iphersuite name: string",
                                                           "add ciphersuite to list of negotiated suites");
      cmd.addOption("--dhparam",                "+dp",  1, "[f]ilename: string",
                                                           "read DH parameters for DH/DSS ciphersuites");
    cmd.addSubGroup("pseudo random generator:");
      cmd.addOption("--seed",                   "+rs",  1, "[f]ilename: string",
                                                           "seed random generator with contents of f");
      cmd.addOption("--write-seed",             "+ws",     "write back modified seed (only with --seed)");
      cmd.addOption("--write-seed-file",        "+wf",  1, "[f]ilename: string (only with --seed)",
                                                           "write modified seed to file f");
    cmd.addSubGroup("peer authentication");
      cmd.addOption("--require-peer-cert",      "-rc",     "verify peer certificate, fail if absent (def.)");
      cmd.addOption("--verify-peer-cert",       "-vc",     "verify peer certificate if present");
      cmd.addOption("--ignore-peer-cert",       "-ic",     "don't verify peer certificate");
#endif

  cmd.addGroup("output options:");
    cmd.addSubGroup("general:");
      cmd.addOption("--output-directory",       "-od",  1, "[d]irectory: string (default: \".\")", "write received objects to existing directory d");
    cmd.addSubGroup("bit preserving mode:");
      cmd.addOption("--normal",                 "-B",      "allow implicit format conversions (default)");
      cmd.addOption("--bit-preserving",         "+B",      "write data exactly as read");
    cmd.addSubGroup("output file format:");
      cmd.addOption("--write-file",             "+F",      "write file format (default)");
      cmd.addOption("--write-dataset",          "-F",      "write data set without file meta information");
    cmd.addSubGroup("output transfer syntax (not with --bit-preserving or compressed transmission):");
      cmd.addOption("--write-xfer-same",        "+t=",     "write with same TS as input (default)");
      cmd.addOption("--write-xfer-little",      "+te",     "write with explicit VR little endian TS");
      cmd.addOption("--write-xfer-big",         "+tb",     "write with explicit VR big endian TS");
      cmd.addOption("--write-xfer-implicit",    "+ti",     "write with implicit VR little endian TS");
#ifdef WITH_ZLIB
      cmd.addOption("--write-xfer-deflated",    "+td",     "write with deflated expl. VR little endian TS");
#endif
    cmd.addSubGroup("post-1993 value representations (not with --bit-preserving):");
      cmd.addOption("--enable-new-vr",          "+u",      "enable support for new VRs (UN/UT) (default)");
      cmd.addOption("--disable-new-vr",         "-u",      "disable support for new VRs, convert to OB");
    cmd.addSubGroup("group length encoding (not with --bit-preserving):");
      cmd.addOption("--group-length-recalc",    "+g=",     "recalculate group lengths if present (default)");
      cmd.addOption("--group-length-create",    "+g",      "always write with group length elements");
      cmd.addOption("--group-length-remove",    "-g",      "always write without group length elements");
    cmd.addSubGroup("length encoding in sequences and items (not with --bit-preserving):");
      cmd.addOption("--length-explicit",        "+e",      "write with explicit lengths (default)");
      cmd.addOption("--length-undefined",       "-e",      "write with undefined lengths");
    cmd.addSubGroup("data set trailing padding (not with --write-dataset or --bit-preserving):");
      cmd.addOption("--padding-off",            "-p",      "no padding (default)");
      cmd.addOption("--padding-create",         "+p",   2, "[f]ile-pad [i]tem-pad: integer",
                                                           "align file on multiple of f bytes and items\non multiple of i bytes");
#ifdef WITH_ZLIB
    cmd.addSubGroup("deflate compression level (only with --write-xfer-deflated/same):");
      cmd.addOption("--compression-level",      "+cl",  1, "[l]evel: integer (default: 6)",
                                                           "0=uncompressed, 1=fastest, 9=best compression");
#endif
    cmd.addSubGroup("sorting into subdirectories (not with --bit-preserving):");
      cmd.addOption("--sort-conc-studies",      "-ss",  1, "[p]refix: string",
                                                           "sort studies using prefix p and a timestamp");
      cmd.addOption("--sort-on-study-uid",      "-su",  1, "[p]refix: string",
                                                           "sort studies using prefix p and the Study\nInstance UID");
      cmd.addOption("--sort-on-patientname",    "-sp",     "sort studies using the Patient's Name and\na timestamp");

    cmd.addSubGroup("filename generation:");
      cmd.addOption("--default-filenames",      "-uf",     "generate filename from instance UID (default)");
      cmd.addOption("--unique-filenames",       "+uf",     "generate unique filenames");
      cmd.addOption("--timenames",              "-tn",     "generate filename from creation time");
      cmd.addOption("--filename-extension",     "-fe",  1, "[e]xtension: string",
                                                           "append e to all filenames");

  cmd.addGroup("event options:", LONGCOL, SHORTCOL + 2);
    cmd.addOption("--exec-on-reception",        "-xcr", 1, "[c]ommand: string",
                                                           "execute command c after having received and\nprocessed one C-STORE-RQ message");
    cmd.addOption("--exec-on-eostudy",          "-xcs", 1, "[c]ommand: string",
                                                           "execute command c after having received and\nprocessed all C-STORE-RQ messages that belong\nto one study");
    cmd.addOption("--rename-on-eostudy",        "-rns",    "having received and processed all C-STORE-RQ\nmessages that belong to one study, rename\noutput files according to certain pattern");
    cmd.addOption("--eostudy-timeout",          "-tos", 1, "[t]imeout: integer",
                                                           "specifies a timeout of t seconds for\nend-of-study determination");
    cmd.addOption("--exec-sync",                "-xs",     "execute command synchronously in foreground");

  /* evaluate command line */
  prepareCmdLineArgs(argc, argv, OFFIS_CONSOLE_APPLICATION);
  if (app.parseCommandLine(cmd, argc, argv))
  {
    /* print help text and exit */
    if (cmd.getArgCount() == 0)
      app.printUsage();

    /* check exclusive options first */
    if (cmd.hasExclusiveOption())
    {
      if (cmd.findOption("--version"))
      {
        app.printHeader(OFTrue /*print host identifier*/);
        COUT << OFendl << "External libraries used:";
#if !defined(WITH_ZLIB) && !defined(WITH_OPENSSL) && !defined(WITH_TCPWRAPPER)
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
#ifdef WITH_TCPWRAPPER
        COUT << "- LIBWRAP" << OFendl;
#endif
        return 0;
      }
    }

#ifdef INETD_AVAILABLE
    if (cmd.findOption("--inetd"))
    {
      opt_inetd_mode = OFTrue;
      opt_forkMode = OFFalse;

      // duplicate stdin, which is the socket passed by inetd
      int inetd_fd = dup(0);
      if (inetd_fd < 0) exit(99);

      close(0); // close stdin
      close(1); // close stdout
      close(2); // close stderr

      // open new file descriptor for stdin
      int fd = open("/dev/null", O_RDONLY);
      if (fd != 0) exit(99);

      // create new file descriptor for stdout
      fd = open("/dev/null", O_WRONLY);
      if (fd != 1) exit(99);

      // create new file descriptor for stderr
      fd = open("/dev/null", O_WRONLY);
      if (fd != 2) exit(99);

      dcmExternalSocketHandle.set(inetd_fd);

      // the port number is not really used. Set to non-privileged port number
      // to avoid failing the privilege test.
      opt_port = 1024;
    }
#endif

#if defined(HAVE_FORK) || defined(_WIN32)
    cmd.beginOptionBlock();
    if (cmd.findOption("--single-process"))
      opt_forkMode = OFFalse;
    if (cmd.findOption("--fork"))
    {
      app.checkConflict("--inetd", "--fork", opt_inetd_mode);
      opt_forkMode = OFTrue;
    }
    cmd.endOptionBlock();
#ifdef _WIN32
    if (cmd.findOption("--forked-child")) opt_forkedChild = OFTrue;
#endif
#endif

    if (opt_inetd_mode)
    {
      // port number is not required in inetd mode
      if (cmd.getParamCount() > 0)
        OFLOG_WARN(storescpLogger, "Parameter port not required in inetd mode");
    } else {
      // omitting the port number is only allowed in inetd mode
      if (cmd.getParamCount() == 0)
        app.printError("Missing parameter port");
      else
        app.checkParam(cmd.getParamAndCheckMinMax(1, opt_port, 1, 65535));
    }

    OFLog::configureFromCommandLine(cmd, app);
    if (cmd.findOption("--verbose-pc"))
    {
      app.checkDependence("--verbose-pc", "verbose mode", storescpLogger.isEnabledFor(OFLogger::INFO_LOG_LEVEL));
      opt_showPresentationContexts = OFTrue;
    }

    cmd.beginOptionBlock();
    if (cmd.findOption("--prefer-uncompr"))
    {
      opt_acceptAllXfers = OFFalse;
      opt_networkTransferSyntax = EXS_Unknown;
    }
    if (cmd.findOption("--prefer-little")) opt_networkTransferSyntax = EXS_LittleEndianExplicit;
    if (cmd.findOption("--prefer-big")) opt_networkTransferSyntax = EXS_BigEndianExplicit;
    if (cmd.findOption("--prefer-lossless")) opt_networkTransferSyntax = EXS_JPEGProcess14SV1;
    if (cmd.findOption("--prefer-jpeg8")) opt_networkTransferSyntax = EXS_JPEGProcess1;
    if (cmd.findOption("--prefer-jpeg12")) opt_networkTransferSyntax = EXS_JPEGProcess2_4;
    if (cmd.findOption("--prefer-j2k-lossless")) opt_networkTransferSyntax = EXS_JPEG2000LosslessOnly;
    if (cmd.findOption("--prefer-j2k-lossy")) opt_networkTransferSyntax = EXS_JPEG2000;
    if (cmd.findOption("--prefer-jls-lossless")) opt_networkTransferSyntax = EXS_JPEGLSLossless;
    if (cmd.findOption("--prefer-jls-lossy")) opt_networkTransferSyntax = EXS_JPEGLSLossy;
    if (cmd.findOption("--prefer-mpeg2")) opt_networkTransferSyntax = EXS_MPEG2MainProfileAtMainLevel;
    if (cmd.findOption("--prefer-mpeg2-high")) opt_networkTransferSyntax = EXS_MPEG2MainProfileAtHighLevel;
    if (cmd.findOption("--prefer-mpeg4")) opt_networkTransferSyntax = EXS_MPEG4HighProfileLevel4_1;
    if (cmd.findOption("--prefer-mpeg4-bd")) opt_networkTransferSyntax = EXS_MPEG4BDcompatibleHighProfileLevel4_1;
    if (cmd.findOption("--prefer-rle")) opt_networkTransferSyntax = EXS_RLELossless;
#ifdef WITH_ZLIB
    if (cmd.findOption("--prefer-deflated")) opt_networkTransferSyntax = EXS_DeflatedLittleEndianExplicit;
#endif
    if (cmd.findOption("--implicit")) opt_networkTransferSyntax = EXS_LittleEndianImplicit;
    if (cmd.findOption("--accept-all"))
    {
      opt_acceptAllXfers = OFTrue;
      opt_networkTransferSyntax = EXS_Unknown;
    }
    cmd.endOptionBlock();
    if (opt_networkTransferSyntax != EXS_Unknown) opt_acceptAllXfers = OFFalse;

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

    if (cmd.findOption("--aetitle")) app.checkValue(cmd.getValue(opt_respondingAETitle));
    if (cmd.findOption("--max-pdu")) app.checkValue(cmd.getValueAndCheckMinMax(opt_maxPDU, ASC_MINIMUMPDUSIZE, ASC_MAXIMUMPDUSIZE));
    if (cmd.findOption("--disable-host-lookup")) dcmDisableGethostbyaddr.set(OFTrue);
    if (cmd.findOption("--refuse")) opt_refuseAssociation = OFTrue;
    if (cmd.findOption("--reject")) opt_rejectWithoutImplementationUID = OFTrue;
    if (cmd.findOption("--ignore")) opt_ignore = OFTrue;
    if (cmd.findOption("--sleep-after")) app.checkValue(cmd.getValueAndCheckMin(opt_sleepAfter, 0));
    if (cmd.findOption("--sleep-during")) app.checkValue(cmd.getValueAndCheckMin(opt_sleepDuring, 0));
    if (cmd.findOption("--abort-after")) opt_abortAfterStore = OFTrue;
    if (cmd.findOption("--abort-during")) opt_abortDuringStore = OFTrue;
    if (cmd.findOption("--promiscuous")) opt_promiscuous = OFTrue;
    if (cmd.findOption("--uid-padding")) opt_correctUIDPadding = OFTrue;

    if (cmd.findOption("--config-file"))
    {
      // check conflicts with other command line options
      app.checkConflict("--config-file", "--prefer-little", opt_networkTransferSyntax == EXS_LittleEndianExplicit);
      app.checkConflict("--config-file", "--prefer-big", opt_networkTransferSyntax == EXS_BigEndianExplicit);
      app.checkConflict("--config-file", "--prefer-lossless", opt_networkTransferSyntax == EXS_JPEGProcess14SV1);
      app.checkConflict("--config-file", "--prefer-jpeg8", opt_networkTransferSyntax == EXS_JPEGProcess1);
      app.checkConflict("--config-file", "--prefer-jpeg12", opt_networkTransferSyntax == EXS_JPEGProcess2_4);
      app.checkConflict("--config-file", "--prefer-j2k-lossless", opt_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
      app.checkConflict("--config-file", "--prefer-j2k-lossy", opt_networkTransferSyntax == EXS_JPEG2000);
      app.checkConflict("--config-file", "--prefer-jls-lossless", opt_networkTransferSyntax == EXS_JPEGLSLossless);
      app.checkConflict("--config-file", "--prefer-jls-lossy", opt_networkTransferSyntax == EXS_JPEGLSLossy);
      app.checkConflict("--config-file", "--prefer-mpeg2", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
      app.checkConflict("--config-file", "--prefer-mpeg2-high", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
      app.checkConflict("--config-file", "--prefer-mpeg4", opt_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
      app.checkConflict("--config-file", "--prefer-mpeg4-bd", opt_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
      app.checkConflict("--config-file", "--prefer-rle", opt_networkTransferSyntax == EXS_RLELossless);
#ifdef WITH_ZLIB
      app.checkConflict("--config-file", "--prefer-deflated", opt_networkTransferSyntax == EXS_DeflatedLittleEndianExplicit);
#endif
      app.checkConflict("--config-file", "--implicit", opt_networkTransferSyntax == EXS_LittleEndianImplicit);
      app.checkConflict("--config-file", "--accept-all", opt_acceptAllXfers);
      app.checkConflict("--config-file", "--promiscuous", opt_promiscuous);

      app.checkValue(cmd.getValue(opt_configFile));
      app.checkValue(cmd.getValue(opt_profileName));

      // read configuration file
      OFCondition cond = DcmAssociationConfigurationFile::initialize(asccfg, opt_configFile);
      if (cond.bad())
      {
        OFLOG_FATAL(storescpLogger, "cannot read config file: " << cond.text());
        return 1;
      }

      /* perform name mangling for config file key */
      OFString sprofile;
      const unsigned char *c = OFreinterpret_cast(const unsigned char *, opt_profileName);
      while (*c)
      {
        if (! isspace(*c)) sprofile += OFstatic_cast(char, toupper(*c));
        ++c;
      }

      if (!asccfg.isKnownProfile(sprofile.c_str()))
      {
        OFLOG_FATAL(storescpLogger, "unknown configuration profile name: " << sprofile);
        return 1;
      }

      if (!asccfg.isValidSCPProfile(sprofile.c_str()))
      {
        OFLOG_FATAL(storescpLogger, "profile '" << sprofile << "' is not valid for SCP use, duplicate abstract syntaxes found");
        return 1;
      }

    }

#ifdef WITH_TCPWRAPPER
    cmd.beginOptionBlock();
    if (cmd.findOption("--access-full")) dcmTCPWrapperDaemonName.set(NULL);
    if (cmd.findOption("--access-control")) dcmTCPWrapperDaemonName.set(OFFIS_CONSOLE_APPLICATION);
    cmd.endOptionBlock();
#endif

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
      app.checkConflict("--write-xfer-little", "--prefer-lossless", opt_networkTransferSyntax == EXS_JPEGProcess14SV1);
      app.checkConflict("--write-xfer-little", "--prefer-jpeg8", opt_networkTransferSyntax == EXS_JPEGProcess1);
      app.checkConflict("--write-xfer-little", "--prefer-jpeg12", opt_networkTransferSyntax == EXS_JPEGProcess2_4);
      app.checkConflict("--write-xfer-little", "--prefer-j2k-lossless", opt_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
      app.checkConflict("--write-xfer-little", "--prefer-j2k-lossy", opt_networkTransferSyntax == EXS_JPEG2000);
      app.checkConflict("--write-xfer-little", "--prefer-jls-lossless", opt_networkTransferSyntax == EXS_JPEGLSLossless);
      app.checkConflict("--write-xfer-little", "--prefer-jls-lossy", opt_networkTransferSyntax == EXS_JPEGLSLossy);
      app.checkConflict("--write-xfer-little", "--prefer-mpeg2", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
      app.checkConflict("--write-xfer-little", "--prefer-mpeg2-high", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
      app.checkConflict("--write-xfer-little", "--prefer-mpeg4", opt_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
      app.checkConflict("--write-xfer-little", "--prefer-mpeg4-bd", opt_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
      app.checkConflict("--write-xfer-little", "--prefer-rle", opt_networkTransferSyntax == EXS_RLELossless);
      // we don't have to check a conflict for --prefer-deflated because we can always convert that to uncompressed.
      opt_writeTransferSyntax = EXS_LittleEndianExplicit;
    }
    if (cmd.findOption("--write-xfer-big"))
    {
      app.checkConflict("--write-xfer-big", "--accept-all", opt_acceptAllXfers);
      app.checkConflict("--write-xfer-big", "--bit-preserving", opt_bitPreserving);
      app.checkConflict("--write-xfer-big", "--prefer-lossless", opt_networkTransferSyntax == EXS_JPEGProcess14SV1);
      app.checkConflict("--write-xfer-big", "--prefer-jpeg8", opt_networkTransferSyntax == EXS_JPEGProcess1);
      app.checkConflict("--write-xfer-big", "--prefer-jpeg12", opt_networkTransferSyntax == EXS_JPEGProcess2_4);
      app.checkConflict("--write-xfer-big", "--prefer-j2k-lossless", opt_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
      app.checkConflict("--write-xfer-big", "--prefer-j2k-lossy", opt_networkTransferSyntax == EXS_JPEG2000);
      app.checkConflict("--write-xfer-big", "--prefer-jls-lossless", opt_networkTransferSyntax == EXS_JPEGLSLossless);
      app.checkConflict("--write-xfer-big", "--prefer-jls-lossy", opt_networkTransferSyntax == EXS_JPEGLSLossy);
      app.checkConflict("--write-xfer-big", "--prefer-mpeg2", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
      app.checkConflict("--write-xfer-big", "--prefer-mpeg2-high", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
      app.checkConflict("--write-xfer-big", "--prefer-mpeg4", opt_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
      app.checkConflict("--write-xfer-big", "--prefer-mpeg4-bd", opt_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
      app.checkConflict("--write-xfer-big", "--prefer-rle", opt_networkTransferSyntax == EXS_RLELossless);
      // we don't have to check a conflict for --prefer-deflated because we can always convert that to uncompressed.
      opt_writeTransferSyntax = EXS_BigEndianExplicit;
    }
    if (cmd.findOption("--write-xfer-implicit"))
    {
      app.checkConflict("--write-xfer-implicit", "--accept-all", opt_acceptAllXfers);
      app.checkConflict("--write-xfer-implicit", "--bit-preserving", opt_bitPreserving);
      app.checkConflict("--write-xfer-implicit", "--prefer-lossless", opt_networkTransferSyntax == EXS_JPEGProcess14SV1);
      app.checkConflict("--write-xfer-implicit", "--prefer-jpeg8", opt_networkTransferSyntax == EXS_JPEGProcess1);
      app.checkConflict("--write-xfer-implicit", "--prefer-jpeg12", opt_networkTransferSyntax == EXS_JPEGProcess2_4);
      app.checkConflict("--write-xfer-implicit", "--prefer-j2k-lossless", opt_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
      app.checkConflict("--write-xfer-implicit", "--prefer-j2k-lossy", opt_networkTransferSyntax == EXS_JPEG2000);
      app.checkConflict("--write-xfer-implicit", "--prefer-jls-lossless", opt_networkTransferSyntax == EXS_JPEGLSLossless);
      app.checkConflict("--write-xfer-implicit", "--prefer-jls-lossy", opt_networkTransferSyntax == EXS_JPEGLSLossy);
      app.checkConflict("--write-xfer-implicit", "--prefer-mpeg2", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
      app.checkConflict("--write-xfer-implicit", "--prefer-mpeg2-high", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
      app.checkConflict("--write-xfer-implicit", "--prefer-mpeg4", opt_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
      app.checkConflict("--write-xfer-implicit", "--prefer-mpeg4-bd", opt_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
      app.checkConflict("--write-xfer-implicit", "--prefer-rle", opt_networkTransferSyntax == EXS_RLELossless);
      // we don't have to check a conflict for --prefer-deflated because we can always convert that to uncompressed.
      opt_writeTransferSyntax = EXS_LittleEndianImplicit;
    }
#ifdef WITH_ZLIB
    if (cmd.findOption("--write-xfer-deflated"))
    {
      app.checkConflict("--write-xfer-deflated", "--accept-all", opt_acceptAllXfers);
      app.checkConflict("--write-xfer-deflated", "--bit-preserving", opt_bitPreserving);
      app.checkConflict("--write-xfer-deflated", "--prefer-lossless", opt_networkTransferSyntax == EXS_JPEGProcess14SV1);
      app.checkConflict("--write-xfer-deflated", "--prefer-jpeg8", opt_networkTransferSyntax == EXS_JPEGProcess1);
      app.checkConflict("--write-xfer-deflated", "--prefer-jpeg12", opt_networkTransferSyntax == EXS_JPEGProcess2_4);
      app.checkConflict("--write-xfer-deflated", "--prefer-j2k-lossless", opt_networkTransferSyntax == EXS_JPEG2000LosslessOnly);
      app.checkConflict("--write-xfer-deflated", "--prefer-j2k-lossy", opt_networkTransferSyntax == EXS_JPEG2000);
      app.checkConflict("--write-xfer-deflated", "--prefer-jls-lossless", opt_networkTransferSyntax == EXS_JPEGLSLossless);
      app.checkConflict("--write-xfer-deflated", "--prefer-jls-lossy", opt_networkTransferSyntax == EXS_JPEGLSLossy);
      app.checkConflict("--write-xfer-deflated", "--prefer-mpeg2", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtMainLevel);
      app.checkConflict("--write-xfer-deflated", "--prefer-mpeg2-high", opt_networkTransferSyntax == EXS_MPEG2MainProfileAtHighLevel);
      app.checkConflict("--write-xfer-deflated", "--prefer-mpeg4", opt_networkTransferSyntax == EXS_MPEG4HighProfileLevel4_1);
      app.checkConflict("--write-xfer-deflated", "--prefer-mpeg4-bd", opt_networkTransferSyntax == EXS_MPEG4BDcompatibleHighProfileLevel4_1);
      app.checkConflict("--write-xfer-deflated", "--prefer-rle", opt_networkTransferSyntax == EXS_RLELossless);
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
      app.checkDependence("--compression-level", "--write-xfer-deflated or --write-xfer-same",
        (opt_writeTransferSyntax == EXS_DeflatedLittleEndianExplicit) || (opt_writeTransferSyntax == EXS_Unknown));
      app.checkValue(cmd.getValueAndCheckMinMax(opt_compressionLevel, 0, 9));
      dcmZlibCompressionLevel.set(OFstatic_cast(int, opt_compressionLevel));
    }
#endif

    cmd.beginOptionBlock();
    if (cmd.findOption("--sort-conc-studies"))
    {
      app.checkConflict("--sort-conc-studies", "--bit-preserving", opt_bitPreserving);
      app.checkValue(cmd.getValue(opt_sortStudyDirPrefix));
      opt_sortStudyMode = ESM_Timestamp;
    }
    if (cmd.findOption("--sort-on-study-uid"))
    {
      app.checkConflict("--sort-on-study-uid", "--bit-preserving", opt_bitPreserving);
      app.checkValue(cmd.getValue(opt_sortStudyDirPrefix));
      opt_sortStudyMode = ESM_StudyInstanceUID;
    }
    if (cmd.findOption("--sort-on-patientname"))
    {
      app.checkConflict("--sort-on-patientname", "--bit-preserving", opt_bitPreserving);
      opt_sortStudyDirPrefix = NULL;
      opt_sortStudyMode = ESM_PatientName;
    }
    cmd.endOptionBlock();

    cmd.beginOptionBlock();
    if (cmd.findOption("--default-filenames")) opt_uniqueFilenames = OFFalse;
    if (cmd.findOption("--unique-filenames")) opt_uniqueFilenames = OFTrue;
    cmd.endOptionBlock();

    if (cmd.findOption("--timenames")) opt_timeNames = OFTrue;
    if (cmd.findOption("--filename-extension"))
      app.checkValue(cmd.getValue(opt_fileNameExtension));
    if (cmd.findOption("--timenames"))
      app.checkConflict("--timenames", "--unique-filenames", opt_uniqueFilenames);

    if (cmd.findOption("--exec-on-reception")) app.checkValue(cmd.getValue(opt_execOnReception));

    if (cmd.findOption("--exec-on-eostudy"))
    {
      app.checkConflict("--exec-on-eostudy", "--fork", opt_forkMode);
      app.checkConflict("--exec-on-eostudy", "--inetd", opt_inetd_mode);
      app.checkDependence("--exec-on-eostudy", "--sort-conc-studies, --sort-on-study-uid or --sort-on-patientname", opt_sortStudyMode != ESM_None );
      app.checkValue(cmd.getValue(opt_execOnEndOfStudy));
    }

    if (cmd.findOption("--rename-on-eostudy"))
    {
      app.checkConflict("--rename-on-eostudy", "--fork", opt_forkMode);
      app.checkConflict("--rename-on-eostudy", "--inetd", opt_inetd_mode);
      app.checkDependence("--rename-on-eostudy", "--sort-conc-studies, --sort-on-study-uid or --sort-on-patientname", opt_sortStudyMode != ESM_None );
      opt_renameOnEndOfStudy = OFTrue;
    }

    if (cmd.findOption("--eostudy-timeout"))
    {
      app.checkDependence("--eostudy-timeout", "--sort-conc-studies, --sort-on-study-uid, --sort-on-patientname, --exec-on-eostudy or --rename-on-eostudy",
        (opt_sortStudyMode != ESM_None) || (opt_execOnEndOfStudy != NULL) || opt_renameOnEndOfStudy);
      app.checkValue(cmd.getValueAndCheckMin(opt_endOfStudyTimeout, 0));
    }

    if (cmd.findOption("--exec-sync")) opt_execSync = OFTrue;
  }

  /* print resource identifier */
  OFLOG_DEBUG(storescpLogger, rcsid << OFendl);

#ifdef WITH_OPENSSL

  cmd.beginOptionBlock();
  if (cmd.findOption("--disable-tls")) opt_secureConnection = OFFalse;
  if (cmd.findOption("--enable-tls"))
  {
    opt_secureConnection = OFTrue;
    app.checkValue(cmd.getValue(opt_privateKeyFile));
    app.checkValue(cmd.getValue(opt_certificateFile));
  }
  cmd.endOptionBlock();

  cmd.beginOptionBlock();
  if (cmd.findOption("--std-passwd"))
  {
    app.checkDependence("--std-passwd", "--enable-tls", opt_secureConnection);
    opt_passwd = NULL;
  }
  if (cmd.findOption("--use-passwd"))
  {
    app.checkDependence("--use-passwd", "--enable-tls", opt_secureConnection);
    app.checkValue(cmd.getValue(opt_passwd));
  }
  if (cmd.findOption("--null-passwd"))
  {
    app.checkDependence("--null-passwd", "--enable-tls", opt_secureConnection);
    opt_passwd = "";
  }
  cmd.endOptionBlock();

  cmd.beginOptionBlock();
  if (cmd.findOption("--pem-keys")) opt_keyFileFormat = SSL_FILETYPE_PEM;
  if (cmd.findOption("--der-keys")) opt_keyFileFormat = SSL_FILETYPE_ASN1;
  cmd.endOptionBlock();

  if (cmd.findOption("--dhparam"))
  {
    app.checkValue(cmd.getValue(opt_dhparam));
  }

  if (cmd.findOption("--seed"))
  {
    app.checkValue(cmd.getValue(opt_readSeedFile));
  }

  cmd.beginOptionBlock();
  if (cmd.findOption("--write-seed"))
  {
    app.checkDependence("--write-seed", "--seed", opt_readSeedFile != NULL);
    opt_writeSeedFile = opt_readSeedFile;
  }
  if (cmd.findOption("--write-seed-file"))
  {
    app.checkDependence("--write-seed-file", "--seed", opt_readSeedFile != NULL);
    app.checkValue(cmd.getValue(opt_writeSeedFile));
  }
  cmd.endOptionBlock();

  cmd.beginOptionBlock();
  if (cmd.findOption("--require-peer-cert")) opt_certVerification = DCV_requireCertificate;
  if (cmd.findOption("--verify-peer-cert"))  opt_certVerification = DCV_checkCertificate;
  if (cmd.findOption("--ignore-peer-cert"))  opt_certVerification = DCV_ignoreCertificate;
  cmd.endOptionBlock();

  const char *current = NULL;
  const char *currentOpenSSL;
  if (cmd.findOption("--cipher", 0, OFCommandLine::FOM_First))
  {
    opt_ciphersuites.clear();
    do
    {
      app.checkValue(cmd.getValue(current));
      if (NULL == (currentOpenSSL = DcmTLSTransportLayer::findOpenSSLCipherSuiteName(current)))
      {
        OFLOG_FATAL(storescpLogger, "ciphersuite '" << current << "' is unknown, known ciphersuites are:");
        unsigned long numSuites = DcmTLSTransportLayer::getNumberOfCipherSuites();
        for (unsigned long cs = 0; cs < numSuites; cs++)
        {
          OFLOG_FATAL(storescpLogger, "    " << DcmTLSTransportLayer::getTLSCipherSuiteName(cs));
        }
        return 1;
      }
      else
      {
        if (!opt_ciphersuites.empty()) opt_ciphersuites += ":";
        opt_ciphersuites += currentOpenSSL;
      }
    } while (cmd.findOption("--cipher", 0, OFCommandLine::FOM_Next));
  }

#endif

#ifndef DISABLE_PORT_PERMISSION_CHECK
#ifdef HAVE_GETEUID
  /* if port is privileged we must be as well */
  if (opt_port < 1024)
  {
    if (geteuid() != 0)
    {
      OFLOG_FATAL(storescpLogger, "cannot listen on port " << opt_port << ", insufficient privileges");
      return 1;
    }
  }
#endif
#endif

  /* make sure data dictionary is loaded */
  if (!dcmDataDict.isDictionaryLoaded())
  {
    OFLOG_WARN(storescpLogger, "no data dictionary loaded, check environment variable: "
        << DCM_DICT_ENVIRONMENT_VARIABLE);
  }

  /* if the output directory does not equal "." (default directory) */
  if (opt_outputDirectory != ".")
  {
    /* if there is a path separator at the end of the path, get rid of it */
    OFStandard::normalizeDirName(opt_outputDirectory, opt_outputDirectory);

    /* check if the specified directory exists and if it is a directory.
     * If the output directory is invalid, dump an error message and terminate execution.
     */
    if (!OFStandard::dirExists(opt_outputDirectory))
    {
      OFLOG_FATAL(storescpLogger, "specified output directory does not exist");
      return 1;
    }
  }

  /* check if the output directory is writeable */
  if (!OFStandard::isWriteable(opt_outputDirectory))
  {
    OFLOG_FATAL(storescpLogger, "specified output directory is not writeable");
    return 1;
  }

#ifdef HAVE_FORK
  if (opt_forkMode)
    DUL_requestForkOnTransportConnectionReceipt(argc, argv);
#elif defined(_WIN32)
  if (opt_forkedChild)
  {
    // child process
    DUL_markProcessAsForkedChild();

    char buf[256];
    DWORD bytesRead = 0;
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

    // read socket handle number from stdin, i.e. the anonymous pipe
    // to which our parent process has written the handle number.
    if (ReadFile(hStdIn, buf, sizeof(buf), &bytesRead, NULL))
    {
      // make sure buffer is zero terminated
      buf[bytesRead] = '\0';
      dcmExternalSocketHandle.set(atoi(buf));
    }
    else
    {
      OFLOG_ERROR(storescpLogger, "cannot read socket handle: " << GetLastError());
      return 1;
    }
  }
  else
  {
    // parent process
    if (opt_forkMode)
      DUL_requestForkOnTransportConnectionReceipt(argc, argv);
  }
#endif

  /* initialize network, i.e. create an instance of T_ASC_Network*. */
  OFCondition cond = ASC_initializeNetwork(NET_ACCEPTOR, OFstatic_cast(int, opt_port), opt_acse_timeout, &net);
  if (cond.bad())
  {
    OFLOG_ERROR(storescpLogger, "cannot create network: " << DimseCondition::dump(temp_str, cond));
    return 1;
  }

#ifdef HAVE_GETUID
  /* return to normal uid so that we can't do too much damage in case
   * things go very wrong.   Only does something if the program is setuid
   * root, and run by another user.  Running as root user may be
   * potentially disastrous if this program screws up badly.
   */
  if ((setuid(getuid()) == -1) && (errno == EAGAIN))
  {
      OFLOG_FATAL(storescpLogger, "setuid() failed, maximum number of processes/threads for uid already running.");
      return 1;
  }
#endif

#ifdef WITH_OPENSSL
  DcmTLSTransportLayer *tLayer = NULL;
  if (opt_secureConnection)
  {
    tLayer = new DcmTLSTransportLayer(DICOM_APPLICATION_ACCEPTOR, opt_readSeedFile);
    if (tLayer == NULL)
    {
      OFLOG_FATAL(storescpLogger, "unable to create TLS transport layer");
      return 1;
    }

    if (cmd.findOption("--add-cert-file", 0, OFCommandLine::FOM_First))
    {
      do
      {
        app.checkValue(cmd.getValue(current));
        if (TCS_ok != tLayer->addTrustedCertificateFile(current, opt_keyFileFormat))
        {
          OFLOG_WARN(storescpLogger, "unable to load certificate file '" << current << "', ignoring");
        }
      } while (cmd.findOption("--add-cert-file", 0, OFCommandLine::FOM_Next));
    }

    if (cmd.findOption("--add-cert-dir", 0, OFCommandLine::FOM_First))
    {
      do
      {
        app.checkValue(cmd.getValue(current));
        if (TCS_ok != tLayer->addTrustedCertificateDir(current, opt_keyFileFormat))
        {
          OFLOG_WARN(storescpLogger, "unable to load certificates from directory '" << current << "', ignoring");
        }
      } while (cmd.findOption("--add-cert-dir", 0, OFCommandLine::FOM_Next));
    }

    if (opt_dhparam && !(tLayer->setTempDHParameters(opt_dhparam)))
    {
      OFLOG_WARN(storescpLogger, "unable to load temporary DH parameter file '" << opt_dhparam << "', ignoring");
    }

    if (opt_passwd) tLayer->setPrivateKeyPasswd(opt_passwd);

    if (TCS_ok != tLayer->setPrivateKeyFile(opt_privateKeyFile, opt_keyFileFormat))
    {
      OFLOG_WARN(storescpLogger, "unable to load private TLS key from '" << opt_privateKeyFile << "'");
      return 1;
    }
    if (TCS_ok != tLayer->setCertificateFile(opt_certificateFile, opt_keyFileFormat))
    {
      OFLOG_WARN(storescpLogger, "unable to load certificate from '" << opt_certificateFile << "'");
      return 1;
    }
    if (! tLayer->checkPrivateKeyMatchesCertificate())
    {
      OFLOG_WARN(storescpLogger, "private key '" << opt_privateKeyFile << "' and certificate '" << opt_certificateFile << "' do not match");
      return 1;
    }

    if (TCS_ok != tLayer->setCipherSuites(opt_ciphersuites.c_str()))
    {
      OFLOG_WARN(storescpLogger, "unable to set selected cipher suites");
      return 1;
    }

    tLayer->setCertificateVerification(opt_certVerification);

    cond = ASC_setTransportLayer(net, tLayer, 0);
    if (cond.bad())
    {
      OFLOG_ERROR(storescpLogger, DimseCondition::dump(temp_str, cond));
      return 1;
    }
  }
#endif

#ifdef HAVE_WAITPID
  // register signal handler
  signal(SIGCHLD, sigChildHandler);
#endif

  while (cond.good())
  {
    /* receive an association and acknowledge or reject it. If the association was */
    /* acknowledged, offer corresponding services and invoke one or more if required. */
    cond = acceptAssociation(net, asccfg);

    /* remove zombie child processes */
    cleanChildren(-1, OFFalse);
#ifdef WITH_OPENSSL
    /* since storescp is usually terminated with SIGTERM or the like,
     * we write back an updated random seed after every association handled.
     */
    if (tLayer && opt_writeSeedFile)
    {
      if (tLayer->canWriteRandomSeed())
      {
        if (!tLayer->writeRandomSeed(opt_writeSeedFile))
          OFLOG_WARN(storescpLogger, "cannot write random seed file '" << opt_writeSeedFile << "', ignoring");
      }
      else
      {
        OFLOG_WARN(storescpLogger, "cannot write random seed, ignoring");
      }
    }
#endif
    // if running in inetd mode, we always terminate after one association
    if (opt_inetd_mode) break;

    // if running in multi-process mode, always terminate child after one association
    if (DUL_processIsForkedChild()) break;
  }

  /* drop the network, i.e. free memory of T_ASC_Network* structure. This call */
  /* is the counterpart of ASC_initializeNetwork(...) which was called above. */
  cond = ASC_dropNetwork(&net);
  if (cond.bad())
  {
    OFLOG_ERROR(storescpLogger, DimseCondition::dump(temp_str, cond));
    return 1;
  }

#ifdef HAVE_WINSOCK_H
  WSACleanup();
#endif

#ifdef WITH_OPENSSL
  delete tLayer;
#endif

  return 0;
}