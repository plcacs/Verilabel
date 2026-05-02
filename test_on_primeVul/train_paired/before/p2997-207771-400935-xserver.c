ddxProcessArgument(int argc, char **argv, int i)
{
#define CHECK_FOR_REQUIRED_ARGUMENT() \
    if (((i + 1) >= argc) || (!argv[i + 1])) { 				\
      ErrorF("Required argument to %s not specified\n", argv[i]); 	\
      UseMsg(); 							\
      FatalError("Required argument to %s not specified\n", argv[i]);	\
    }

    /* First the options that are not allowed with elevated privileges */
    if (!strcmp(argv[i], "-modulepath")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86CheckPrivs(argv[i], argv[i + 1]);
        xf86ModulePath = argv[i + 1];
        xf86ModPathFrom = X_CMDLINE;
        return 2;
    }
    if (!strcmp(argv[i], "-logfile")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86CheckPrivs(argv[i], argv[i + 1]);
        xf86LogFile = argv[i + 1];
        xf86LogFileFrom = X_CMDLINE;
        return 2;
    }
    if (!strcmp(argv[i], "-config") || !strcmp(argv[i], "-xf86config")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86CheckPrivs(argv[i], argv[i + 1]);
        xf86ConfigFile = argv[i + 1];
        return 2;
    }
    if (!strcmp(argv[i], "-configdir")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86CheckPrivs(argv[i], argv[i + 1]);
        xf86ConfigDir = argv[i + 1];
        return 2;
    }
    if (!strcmp(argv[i], "-flipPixels")) {
        xf86FlipPixels = TRUE;
        return 1;
    }
#ifdef XF86VIDMODE
    if (!strcmp(argv[i], "-disableVidMode")) {
        xf86VidModeDisabled = TRUE;
        return 1;
    }
    if (!strcmp(argv[i], "-allowNonLocalXvidtune")) {
        xf86VidModeAllowNonLocal = TRUE;
        return 1;
    }
#endif
    if (!strcmp(argv[i], "-allowMouseOpenFail")) {
        xf86AllowMouseOpenFail = TRUE;
        return 1;
    }
    if (!strcmp(argv[i], "-ignoreABI")) {
        LoaderSetOptions(LDR_OPT_ABI_MISMATCH_NONFATAL);
        return 1;
    }
    if (!strcmp(argv[i], "-verbose")) {
        if (++i < argc && argv[i]) {
            char *end;
            long val;

            val = strtol(argv[i], &end, 0);
            if (*end == '\0') {
                xf86SetVerbosity(val);
                return 2;
            }
        }
        xf86SetVerbosity(++xf86Verbose);
        return 1;
    }
    if (!strcmp(argv[i], "-logverbose")) {
        if (++i < argc && argv[i]) {
            char *end;
            long val;

            val = strtol(argv[i], &end, 0);
            if (*end == '\0') {
                xf86SetLogVerbosity(val);
                return 2;
            }
        }
        xf86SetLogVerbosity(++xf86LogVerbose);
        return 1;
    }
    if (!strcmp(argv[i], "-quiet")) {
        xf86SetVerbosity(-1);
        return 1;
    }
    if (!strcmp(argv[i], "-showconfig") || !strcmp(argv[i], "-version")) {
        xf86PrintBanner();
        exit(0);
    }
    if (!strcmp(argv[i], "-showDefaultModulePath")) {
        xf86PrintDefaultModulePath();
        exit(0);
    }
    if (!strcmp(argv[i], "-showDefaultLibPath")) {
        xf86PrintDefaultLibraryPath();
        exit(0);
    }
    /* Notice the -fp flag, but allow it to pass to the dix layer */
    if (!strcmp(argv[i], "-fp")) {
        xf86fpFlag = TRUE;
        return 0;
    }
    /* Notice the -bs flag, but allow it to pass to the dix layer */
    if (!strcmp(argv[i], "-bs")) {
        xf86bsDisableFlag = TRUE;
        return 0;
    }
    /* Notice the +bs flag, but allow it to pass to the dix layer */
    if (!strcmp(argv[i], "+bs")) {
        xf86bsEnableFlag = TRUE;
        return 0;
    }
    /* Notice the -s flag, but allow it to pass to the dix layer */
    if (!strcmp(argv[i], "-s")) {
        xf86sFlag = TRUE;
        return 0;
    }
    if (!strcmp(argv[i], "-pixmap32") || !strcmp(argv[i], "-pixmap24")) {
        /* silently accept */
        return 1;
    }
    if (!strcmp(argv[i], "-fbbpp")) {
        int bpp;

        CHECK_FOR_REQUIRED_ARGUMENT();
        if (sscanf(argv[++i], "%d", &bpp) == 1) {
            xf86FbBpp = bpp;
            return 2;
        }
        else {
            ErrorF("Invalid fbbpp\n");
            return 0;
        }
    }
    if (!strcmp(argv[i], "-depth")) {
        int depth;

        CHECK_FOR_REQUIRED_ARGUMENT();
        if (sscanf(argv[++i], "%d", &depth) == 1) {
            xf86Depth = depth;
            return 2;
        }
        else {
            ErrorF("Invalid depth\n");
            return 0;
        }
    }
    if (!strcmp(argv[i], "-weight")) {
        int red, green, blue;

        CHECK_FOR_REQUIRED_ARGUMENT();
        if (sscanf(argv[++i], "%1d%1d%1d", &red, &green, &blue) == 3) {
            xf86Weight.red = red;
            xf86Weight.green = green;
            xf86Weight.blue = blue;
            return 2;
        }
        else {
            ErrorF("Invalid weighting\n");
            return 0;
        }
    }
    if (!strcmp(argv[i], "-gamma") || !strcmp(argv[i], "-rgamma") ||
        !strcmp(argv[i], "-ggamma") || !strcmp(argv[i], "-bgamma")) {
        double gamma;

        CHECK_FOR_REQUIRED_ARGUMENT();
        if (sscanf(argv[++i], "%lf", &gamma) == 1) {
            if (gamma < GAMMA_MIN || gamma > GAMMA_MAX) {
                ErrorF("gamma out of range, only  %.2f <= gamma_value <= %.1f"
                       " is valid\n", GAMMA_MIN, GAMMA_MAX);
                return 0;
            }
            if (!strcmp(argv[i - 1], "-gamma"))
                xf86Gamma.red = xf86Gamma.green = xf86Gamma.blue = gamma;
            else if (!strcmp(argv[i - 1], "-rgamma"))
                xf86Gamma.red = gamma;
            else if (!strcmp(argv[i - 1], "-ggamma"))
                xf86Gamma.green = gamma;
            else if (!strcmp(argv[i - 1], "-bgamma"))
                xf86Gamma.blue = gamma;
            return 2;
        }
    }
    if (!strcmp(argv[i], "-layout")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86LayoutName = argv[++i];
        return 2;
    }
    if (!strcmp(argv[i], "-screen")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86ScreenName = argv[++i];
        return 2;
    }
    if (!strcmp(argv[i], "-pointer")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86PointerName = argv[++i];
        return 2;
    }
    if (!strcmp(argv[i], "-keyboard")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        xf86KeyboardName = argv[++i];
        return 2;
    }
    if (!strcmp(argv[i], "-nosilk")) {
        xf86silkenMouseDisableFlag = TRUE;
        return 1;
    }
#ifdef HAVE_ACPI
    if (!strcmp(argv[i], "-noacpi")) {
        xf86acpiDisableFlag = TRUE;
        return 1;
    }
#endif
    if (!strcmp(argv[i], "-configure")) {
        if (getuid() != 0 && geteuid() == 0) {
            ErrorF("The '-configure' option can only be used by root.\n");
            exit(1);
        }
        xf86DoConfigure = TRUE;
        xf86AllowMouseOpenFail = TRUE;
        return 1;
    }
    if (!strcmp(argv[i], "-showopts")) {
        if (getuid() != 0 && geteuid() == 0) {
            ErrorF("The '-showopts' option can only be used by root.\n");
            exit(1);
        }
        xf86DoShowOptions = TRUE;
        return 1;
    }
#ifdef XSERVER_LIBPCIACCESS
    if (!strcmp(argv[i], "-isolateDevice")) {
        CHECK_FOR_REQUIRED_ARGUMENT();
        if (strncmp(argv[++i], "PCI:", 4)) {
            FatalError("Bus types other than PCI not yet isolable\n");
        }
        xf86PciIsolateDevice(argv[i]);
        return 2;
    }
#endif
    /* Notice cmdline xkbdir, but pass to dix as well */
    if (!strcmp(argv[i], "-xkbdir")) {
        xf86xkbdirFlag = TRUE;
        return 0;
    }
    if (!strcmp(argv[i], "-novtswitch")) {
        xf86Info.autoVTSwitch = FALSE;
        return 1;
    }
    if (!strcmp(argv[i], "-sharevts")) {
        xf86Info.ShareVTs = TRUE;
        return 1;
    }
    if (!strcmp(argv[i], "-iglx") || !strcmp(argv[i], "+iglx")) {
        xf86Info.iglxFrom = X_CMDLINE;
        return 0;
    }

    /* OS-specific processing */
    return xf86ProcessArgument(argc, argv, i);
}