_ppdCreateFromIPP(char   *buffer,	/* I - Filename buffer */
                  size_t bufsize,	/* I - Size of filename buffer */
		  ipp_t  *response)	/* I - Get-Printer-Attributes response */
{
  cups_file_t		*fp;		/* PPD file */
  cups_array_t		*sizes;		/* Media sizes we've added */
  ipp_attribute_t	*attr,		/* xxx-supported */
			*defattr,	/* xxx-default */
			*x_dim, *y_dim;	/* Media dimensions */
  ipp_t			*media_size;	/* Media size collection */
  char			make[256],	/* Make and model */
			*model,		/* Model name */
			ppdname[PPD_MAX_NAME];
		    			/* PPD keyword */
  int			i, j,		/* Looping vars */
			count,		/* Number of values */
			bottom,		/* Largest bottom margin */
			left,		/* Largest left margin */
			right,		/* Largest right margin */
			top,		/* Largest top margin */
			is_apple = 0,	/* Does the printer support Apple raster? */
			is_pdf = 0,	/* Does the printer support PDF? */
			is_pwg = 0;	/* Does the printer support PWG Raster? */
  pwg_media_t		*pwg;		/* PWG media size */
  int			xres, yres;	/* Resolution values */
  cups_lang_t		*lang = cupsLangDefault();
					/* Localization info */
  struct lconv		*loc = localeconv();
					/* Locale data */
  static const char * const finishings[][2] =
  {					/* Finishings strings */
    { "bale", _("Bale") },
    { "bind", _("Bind") },
    { "bind-bottom", _("Bind (Reverse Landscape)") },
    { "bind-left", _("Bind (Portrait)") },
    { "bind-right", _("Bind (Reverse Portrait)") },
    { "bind-top", _("Bind (Landscape)") },
    { "booklet-maker", _("Booklet Maker") },
    { "coat", _("Coat") },
    { "cover", _("Cover") },
    { "edge-stitch", _("Staple Edge") },
    { "edge-stitch-bottom", _("Staple Edge (Reverse Landscape)") },
    { "edge-stitch-left", _("Staple Edge (Portrait)") },
    { "edge-stitch-right", _("Staple Edge (Reverse Portrait)") },
    { "edge-stitch-top", _("Staple Edge (Landscape)") },
    { "fold", _("Fold") },
    { "fold-accordian", _("Accordian Fold") },
    { "fold-double-gate", _("Double Gate Fold") },
    { "fold-engineering-z", _("Engineering Z Fold") },
    { "fold-gate", _("Gate Fold") },
    { "fold-half", _("Half Fold") },
    { "fold-half-z", _("Half Z Fold") },
    { "fold-left-gate", _("Left Gate Fold") },
    { "fold-letter", _("Letter Fold") },
    { "fold-parallel", _("Parallel Fold") },
    { "fold-poster", _("Poster Fold") },
    { "fold-right-gate", _("Right Gate Fold") },
    { "fold-z", _("Z Fold") },
    { "jog-offset", _("Jog") },
    { "laminate", _("Laminate") },
    { "punch", _("Punch") },
    { "punch-bottom-left", _("Single Punch (Reverse Landscape)") },
    { "punch-bottom-right", _("Single Punch (Reverse Portrait)") },
    { "punch-double-bottom", _("2-Hole Punch (Reverse Portrait)") },
    { "punch-double-left", _("2-Hole Punch (Reverse Landscape)") },
    { "punch-double-right", _("2-Hole Punch (Landscape)") },
    { "punch-double-top", _("2-Hole Punch (Portrait)") },
    { "punch-quad-bottom", _("4-Hole Punch (Reverse Landscape)") },
    { "punch-quad-left", _("4-Hole Punch (Portrait)") },
    { "punch-quad-right", _("4-Hole Punch (Reverse Portrait)") },
    { "punch-quad-top", _("4-Hole Punch (Landscape)") },
    { "punch-top-left", _("Single Punch (Portrait)") },
    { "punch-top-right", _("Single Punch (Landscape)") },
    { "punch-triple-bottom", _("3-Hole Punch (Reverse Landscape)") },
    { "punch-triple-left", _("3-Hole Punch (Portrait)") },
    { "punch-triple-right", _("3-Hole Punch (Reverse Portrait)") },
    { "punch-triple-top", _("3-Hole Punch (Landscape)") },
    { "punch-multiple-bottom", _("Multi-Hole Punch (Reverse Landscape)") },
    { "punch-multiple-left", _("Multi-Hole Punch (Portrait)") },
    { "punch-multiple-right", _("Multi-Hole Punch (Reverse Portrait)") },
    { "punch-multiple-top", _("Multi-Hole Punch (Landscape)") },
    { "saddle-stitch", _("Saddle Stitch") },
    { "staple", _("Staple") },
    { "staple-bottom-left", _("Single Staple (Reverse Landscape)") },
    { "staple-bottom-right", _("Single Staple (Reverse Portrait)") },
    { "staple-dual-bottom", _("Double Staple (Reverse Landscape)") },
    { "staple-dual-left", _("Double Staple (Portrait)") },
    { "staple-dual-right", _("Double Staple (Reverse Portrait)") },
    { "staple-dual-top", _("Double Staple (Landscape)") },
    { "staple-top-left", _("Single Staple (Portrait)") },
    { "staple-top-right", _("Single Staple (Landscape)") },
    { "staple-triple-bottom", _("Triple Staple (Reverse Landscape)") },
    { "staple-triple-left", _("Triple Staple (Portrait)") },
    { "staple-triple-right", _("Triple Staple (Reverse Portrait)") },
    { "staple-triple-top", _("Triple Staple (Landscape)") },
    { "trim", _("Cut Media") }
  };


 /*
  * Range check input...
  */

  if (buffer)
    *buffer = '\0';

  if (!buffer || bufsize < 1 || !response)
    return (NULL);

 /*
  * Open a temporary file for the PPD...
  */

  if ((fp = cupsTempFile2(buffer, (int)bufsize)) == NULL)
    return (NULL);

 /*
  * Standard stuff for PPD file...
  */

  cupsFilePuts(fp, "*PPD-Adobe: \"4.3\"\n");
  cupsFilePuts(fp, "*FormatVersion: \"4.3\"\n");
  cupsFilePrintf(fp, "*FileVersion: \"%d.%d\"\n", CUPS_VERSION_MAJOR, CUPS_VERSION_MINOR);
  cupsFilePuts(fp, "*LanguageVersion: English\n");
  cupsFilePuts(fp, "*LanguageEncoding: ISOLatin1\n");
  cupsFilePuts(fp, "*PSVersion: \"(3010.000) 0\"\n");
  cupsFilePuts(fp, "*LanguageLevel: \"3\"\n");
  cupsFilePuts(fp, "*FileSystem: False\n");
  cupsFilePuts(fp, "*PCFileName: \"ippeve.ppd\"\n");

  if ((attr = ippFindAttribute(response, "printer-make-and-model", IPP_TAG_TEXT)) != NULL)
    strlcpy(make, ippGetString(attr, 0, NULL), sizeof(make));
  else
    strlcpy(make, "Unknown Printer", sizeof(make));

  if (!_cups_strncasecmp(make, "Hewlett Packard ", 16) ||
      !_cups_strncasecmp(make, "Hewlett-Packard ", 16))
  {
    model = make + 16;
    strlcpy(make, "HP", sizeof(make));
  }
  else if ((model = strchr(make, ' ')) != NULL)
    *model++ = '\0';
  else
    model = make;

  cupsFilePrintf(fp, "*Manufacturer: \"%s\"\n", make);
  cupsFilePrintf(fp, "*ModelName: \"%s\"\n", model);
  cupsFilePrintf(fp, "*Product: \"(%s)\"\n", model);
  cupsFilePrintf(fp, "*NickName: \"%s\"\n", model);
  cupsFilePrintf(fp, "*ShortNickName: \"%s\"\n", model);

  if ((attr = ippFindAttribute(response, "color-supported", IPP_TAG_BOOLEAN)) != NULL && ippGetBoolean(attr, 0))
    cupsFilePuts(fp, "*ColorDevice: True\n");
  else
    cupsFilePuts(fp, "*ColorDevice: False\n");

  cupsFilePrintf(fp, "*cupsVersion: %d.%d\n", CUPS_VERSION_MAJOR, CUPS_VERSION_MINOR);
  cupsFilePuts(fp, "*cupsSNMPSupplies: False\n");
  cupsFilePuts(fp, "*cupsLanguages: \"en\"\n");

 /*
  * Filters...
  */

  if ((attr = ippFindAttribute(response, "document-format-supported", IPP_TAG_MIMETYPE)) != NULL)
  {
    is_apple = ippContainsString(attr, "image/urf");
    is_pdf   = ippContainsString(attr, "application/pdf");
    is_pwg   = ippContainsString(attr, "image/pwg-raster");

    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      const char *format = ippGetString(attr, i, NULL);
					/* PDL */

     /*
      * Write cupsFilter2 lines for supported formats, except for PostScript
      * (which has compatibility problems over IPP with some vendors), text,
      * TIFF, and vendor MIME media types...
      */

      if (!_cups_strcasecmp(format, "application/pdf"))
        cupsFilePuts(fp, "*cupsFilter2: \"application/vnd.cups-pdf application/pdf 10 -\"\n");
      else if (!_cups_strcasecmp(format, "image/jpeg") || !_cups_strcasecmp(format, "image/png"))
        cupsFilePrintf(fp, "*cupsFilter2: \"%s %s 0 -\"\n", format, format);
      else if (!_cups_strcasecmp(format, "image/pwg-raster") || !_cups_strcasecmp(format, "image/urf"))
        cupsFilePrintf(fp, "*cupsFilter2: \"%s %s 100 -\"\n", format, format);
      else if (_cups_strcasecmp(format, "application/octet-stream") && _cups_strcasecmp(format, "application/postscript") && _cups_strncasecmp(format, "application/vnd.", 16) && _cups_strncasecmp(format, "image/vnd.", 10) && _cups_strcasecmp(format, "image/tiff") && _cups_strncasecmp(format, "text/", 5))
        cupsFilePrintf(fp, "*cupsFilter2: \"%s %s 10 -\"\n", format, format);
    }
  }

  if (!is_apple && !is_pdf && !is_pwg)
    goto bad_ppd;

 /*
  * PageSize/PageRegion/ImageableArea/PaperDimension
  */

  if ((attr = ippFindAttribute(response, "media-bottom-margin-supported", IPP_TAG_INTEGER)) != NULL)
  {
    for (i = 1, bottom = ippGetInteger(attr, 0), count = ippGetCount(attr); i < count; i ++)
      if (ippGetInteger(attr, i) > bottom)
        bottom = ippGetInteger(attr, i);
  }
  else
    bottom = 1270;

  if ((attr = ippFindAttribute(response, "media-left-margin-supported", IPP_TAG_INTEGER)) != NULL)
  {
    for (i = 1, left = ippGetInteger(attr, 0), count = ippGetCount(attr); i < count; i ++)
      if (ippGetInteger(attr, i) > left)
        left = ippGetInteger(attr, i);
  }
  else
    left = 635;

  if ((attr = ippFindAttribute(response, "media-right-margin-supported", IPP_TAG_INTEGER)) != NULL)
  {
    for (i = 1, right = ippGetInteger(attr, 0), count = ippGetCount(attr); i < count; i ++)
      if (ippGetInteger(attr, i) > right)
        right = ippGetInteger(attr, i);
  }
  else
    right = 635;

  if ((attr = ippFindAttribute(response, "media-top-margin-supported", IPP_TAG_INTEGER)) != NULL)
  {
    for (i = 1, top = ippGetInteger(attr, 0), count = ippGetCount(attr); i < count; i ++)
      if (ippGetInteger(attr, i) > top)
        top = ippGetInteger(attr, i);
  }
  else
    top = 1270;

  if ((defattr = ippFindAttribute(response, "media-col-default", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    if ((attr = ippFindAttribute(ippGetCollection(defattr, 0), "media-size", IPP_TAG_BEGIN_COLLECTION)) != NULL)
    {
      media_size = ippGetCollection(attr, 0);
      x_dim      = ippFindAttribute(media_size, "x-dimension", IPP_TAG_INTEGER);
      y_dim      = ippFindAttribute(media_size, "y-dimension", IPP_TAG_INTEGER);

      if (x_dim && y_dim && (pwg = pwgMediaForSize(ippGetInteger(x_dim, 0), ippGetInteger(y_dim, 0))) != NULL)
	strlcpy(ppdname, pwg->ppd, sizeof(ppdname));
      else
	strlcpy(ppdname, "Unknown", sizeof(ppdname));
    }
    else
      strlcpy(ppdname, "Unknown", sizeof(ppdname));
  }
  else if ((pwg = pwgMediaForPWG(ippGetString(ippFindAttribute(response, "media-default", IPP_TAG_ZERO), 0, NULL))) != NULL)
    strlcpy(ppdname, pwg->ppd, sizeof(ppdname));
  else
    strlcpy(ppdname, "Unknown", sizeof(ppdname));

  if ((attr = ippFindAttribute(response, "media-size-supported", IPP_TAG_BEGIN_COLLECTION)) == NULL)
    attr = ippFindAttribute(response, "media-supported", IPP_TAG_ZERO);
  if (attr)
  {
    cupsFilePrintf(fp, "*OpenUI *PageSize: PickOne\n"
		       "*OrderDependency: 10 AnySetup *PageSize\n"
                       "*DefaultPageSize: %s\n", ppdname);

    sizes = cupsArrayNew3((cups_array_func_t)strcmp, NULL, NULL, 0, (cups_acopy_func_t)strdup, (cups_afree_func_t)free);

    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      if (ippGetValueTag(attr) == IPP_TAG_BEGIN_COLLECTION)
      {
	media_size = ippGetCollection(attr, i);
	x_dim      = ippFindAttribute(media_size, "x-dimension", IPP_TAG_INTEGER);
	y_dim      = ippFindAttribute(media_size, "y-dimension", IPP_TAG_INTEGER);

	pwg = pwgMediaForSize(ippGetInteger(x_dim, 0), ippGetInteger(y_dim, 0));
      }
      else
        pwg = pwgMediaForPWG(ippGetString(attr, i, NULL));

      if (pwg)
      {
        char	twidth[256],		/* Width string */
		tlength[256];		/* Length string */

        if (cupsArrayFind(sizes, (void *)pwg->ppd))
        {
          cupsFilePrintf(fp, "*%% warning: Duplicate size '%s' reported by printer.\n", pwg->ppd);
          continue;
        }

        cupsArrayAdd(sizes, (void *)pwg->ppd);

        _cupsStrFormatd(twidth, twidth + sizeof(twidth), pwg->width * 72.0 / 2540.0, loc);
        _cupsStrFormatd(tlength, tlength + sizeof(tlength), pwg->length * 72.0 / 2540.0, loc);

        cupsFilePrintf(fp, "*PageSize %s: \"<</PageSize[%s %s]>>setpagedevice\"\n", pwg->ppd, twidth, tlength);
      }
    }
    cupsFilePuts(fp, "*CloseUI: *PageSize\n");

    cupsArrayDelete(sizes);
    sizes = cupsArrayNew3((cups_array_func_t)strcmp, NULL, NULL, 0, (cups_acopy_func_t)strdup, (cups_afree_func_t)free);

    cupsFilePrintf(fp, "*OpenUI *PageRegion: PickOne\n"
                       "*OrderDependency: 10 AnySetup *PageRegion\n"
                       "*DefaultPageRegion: %s\n", ppdname);
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      if (ippGetValueTag(attr) == IPP_TAG_BEGIN_COLLECTION)
      {
	media_size = ippGetCollection(attr, i);
	x_dim      = ippFindAttribute(media_size, "x-dimension", IPP_TAG_INTEGER);
	y_dim      = ippFindAttribute(media_size, "y-dimension", IPP_TAG_INTEGER);

	pwg = pwgMediaForSize(ippGetInteger(x_dim, 0), ippGetInteger(y_dim, 0));
      }
      else
        pwg = pwgMediaForPWG(ippGetString(attr, i, NULL));

      if (pwg)
      {
        char	twidth[256],		/* Width string */
		tlength[256];		/* Length string */

        if (cupsArrayFind(sizes, (void *)pwg->ppd))
          continue;

        cupsArrayAdd(sizes, (void *)pwg->ppd);

        _cupsStrFormatd(twidth, twidth + sizeof(twidth), pwg->width * 72.0 / 2540.0, loc);
        _cupsStrFormatd(tlength, tlength + sizeof(tlength), pwg->length * 72.0 / 2540.0, loc);

        cupsFilePrintf(fp, "*PageRegion %s: \"<</PageSize[%s %s]>>setpagedevice\"\n", pwg->ppd, twidth, tlength);
      }
    }
    cupsFilePuts(fp, "*CloseUI: *PageRegion\n");

    cupsArrayDelete(sizes);
    sizes = cupsArrayNew3((cups_array_func_t)strcmp, NULL, NULL, 0, (cups_acopy_func_t)strdup, (cups_afree_func_t)free);

    cupsFilePrintf(fp, "*DefaultImageableArea: %s\n"
		       "*DefaultPaperDimension: %s\n", ppdname, ppdname);
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      if (ippGetValueTag(attr) == IPP_TAG_BEGIN_COLLECTION)
      {
	media_size = ippGetCollection(attr, i);
	x_dim      = ippFindAttribute(media_size, "x-dimension", IPP_TAG_INTEGER);
	y_dim      = ippFindAttribute(media_size, "y-dimension", IPP_TAG_INTEGER);

	pwg = pwgMediaForSize(ippGetInteger(x_dim, 0), ippGetInteger(y_dim, 0));
      }
      else
        pwg = pwgMediaForPWG(ippGetString(attr, i, NULL));

      if (pwg)
      {
        char	tleft[256],		/* Left string */
		tbottom[256],		/* Bottom string */
		tright[256],		/* Right string */
		ttop[256],		/* Top string */
		twidth[256],		/* Width string */
		tlength[256];		/* Length string */

        if (cupsArrayFind(sizes, (void *)pwg->ppd))
          continue;

        cupsArrayAdd(sizes, (void *)pwg->ppd);

        _cupsStrFormatd(tleft, tleft + sizeof(tleft), left * 72.0 / 2540.0, loc);
        _cupsStrFormatd(tbottom, tbottom + sizeof(tbottom), bottom * 72.0 / 2540.0, loc);
        _cupsStrFormatd(tright, tright + sizeof(tright), (pwg->width - right) * 72.0 / 2540.0, loc);
        _cupsStrFormatd(ttop, ttop + sizeof(ttop), (pwg->length - top) * 72.0 / 2540.0, loc);
        _cupsStrFormatd(twidth, twidth + sizeof(twidth), pwg->width * 72.0 / 2540.0, loc);
        _cupsStrFormatd(tlength, tlength + sizeof(tlength), pwg->length * 72.0 / 2540.0, loc);

        cupsFilePrintf(fp, "*ImageableArea %s: \"%s %s %s %s\"\n", pwg->ppd, tleft, tbottom, tright, ttop);
        cupsFilePrintf(fp, "*PaperDimension %s: \"%s %s\"\n", pwg->ppd, twidth, tlength);
      }
    }

    cupsArrayDelete(sizes);
  }
  else
    goto bad_ppd;

 /*
  * InputSlot...
  */

  if ((attr = ippFindAttribute(ippGetCollection(defattr, 0), "media-source", IPP_TAG_ZERO)) != NULL)
    pwg_ppdize_name(ippGetString(attr, 0, NULL), ppdname, sizeof(ppdname));
  else
    strlcpy(ppdname, "Unknown", sizeof(ppdname));

  if ((attr = ippFindAttribute(response, "media-source-supported", IPP_TAG_ZERO)) != NULL && (count = ippGetCount(attr)) > 1)
  {
    static const char * const sources[][2] =
    {					/* "media-source" strings */
      { "Auto", _("Automatic") },
      { "Main", _("Main") },
      { "Alternate", _("Alternate") },
      { "LargeCapacity", _("Large Capacity") },
      { "Manual", _("Manual") },
      { "Envelope", _("Envelope") },
      { "Disc", _("Disc") },
      { "Photo", _("Photo") },
      { "Hagaki", _("Hagaki") },
      { "MainRoll", _("Main Roll") },
      { "AlternateRoll", _("Alternate Roll") },
      { "Top", _("Top") },
      { "Middle", _("Middle") },
      { "Bottom", _("Bottom") },
      { "Side", _("Side") },
      { "Left", _("Left") },
      { "Right", _("Right") },
      { "Center", _("Center") },
      { "Rear", _("Rear") },
      { "ByPassTray", _("Multipurpose") },
      { "Tray1", _("Tray 1") },
      { "Tray2", _("Tray 2") },
      { "Tray3", _("Tray 3") },
      { "Tray4", _("Tray 4") },
      { "Tray5", _("Tray 5") },
      { "Tray6", _("Tray 6") },
      { "Tray7", _("Tray 7") },
      { "Tray8", _("Tray 8") },
      { "Tray9", _("Tray 9") },
      { "Tray10", _("Tray 10") },
      { "Tray11", _("Tray 11") },
      { "Tray12", _("Tray 12") },
      { "Tray13", _("Tray 13") },
      { "Tray14", _("Tray 14") },
      { "Tray15", _("Tray 15") },
      { "Tray16", _("Tray 16") },
      { "Tray17", _("Tray 17") },
      { "Tray18", _("Tray 18") },
      { "Tray19", _("Tray 19") },
      { "Tray20", _("Tray 20") },
      { "Roll1", _("Roll 1") },
      { "Roll2", _("Roll 2") },
      { "Roll3", _("Roll 3") },
      { "Roll4", _("Roll 4") },
      { "Roll5", _("Roll 5") },
      { "Roll6", _("Roll 6") },
      { "Roll7", _("Roll 7") },
      { "Roll8", _("Roll 8") },
      { "Roll9", _("Roll 9") },
      { "Roll10", _("Roll 10") }
    };

    cupsFilePrintf(fp, "*OpenUI *InputSlot: PickOne\n"
                       "*OrderDependency: 10 AnySetup *InputSlot\n"
                       "*DefaultInputSlot: %s\n", ppdname);
    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      pwg_ppdize_name(ippGetString(attr, i, NULL), ppdname, sizeof(ppdname));

      for (j = 0; j < (int)(sizeof(sources) / sizeof(sources[0])); j ++)
        if (!strcmp(sources[j][0], ppdname))
	{
	  cupsFilePrintf(fp, "*InputSlot %s/%s: \"<</MediaPosition %d>>setpagedevice\"\n", ppdname, _cupsLangString(lang, sources[j][1]), j);
	  break;
	}
    }
    cupsFilePuts(fp, "*CloseUI: *InputSlot\n");
  }

 /*
  * MediaType...
  */

  if ((attr = ippFindAttribute(ippGetCollection(defattr, 0), "media-type", IPP_TAG_ZERO)) != NULL)
    pwg_ppdize_name(ippGetString(attr, 0, NULL), ppdname, sizeof(ppdname));
  else
    strlcpy(ppdname, "Unknown", sizeof(ppdname));

  if ((attr = ippFindAttribute(response, "media-type-supported", IPP_TAG_ZERO)) != NULL && (count = ippGetCount(attr)) > 1)
  {
    static const char * const media_types[][2] =
    {					/* "media-type" strings */
      { "aluminum", _("Aluminum") },
      { "auto", _("Automatic") },
      { "back-print-film", _("Back Print Film") },
      { "cardboard", _("Cardboard") },
      { "cardstock", _("Cardstock") },
      { "cd", _("CD") },
      { "continuous", _("Continuous") },
      { "continuous-long", _("Continuous Long") },
      { "continuous-short", _("Continuous Short") },
      { "disc", _("Optical Disc") },
      { "disc-glossy", _("Glossy Optical Disc") },
      { "disc-high-gloss", _("High Gloss Optical Disc") },
      { "disc-matte", _("Matte Optical Disc") },
      { "disc-satin", _("Satin Optical Disc") },
      { "disc-semi-gloss", _("Semi-Gloss Optical Disc") },
      { "double-wall", _("Double Wall Cardboard") },
      { "dry-film", _("Dry Film") },
      { "dvd", _("DVD") },
      { "embossing-foil", _("Embossing Foil") },
      { "end-board", _("End Board") },
      { "envelope", _("Envelope") },
      { "envelope-archival", _("Archival Envelope") },
      { "envelope-bond", _("Bond Envelope") },
      { "envelope-coated", _("Coated Envelope") },
      { "envelope-cotton", _("Cotton Envelope") },
      { "envelope-fine", _("Fine Envelope") },
      { "envelope-heavyweight", _("Heavyweight Envelope") },
      { "envelope-inkjet", _("Inkjet Envelope") },
      { "envelope-lightweight", _("Lightweight Envelope") },
      { "envelope-plain", _("Plain Envelope") },
      { "envelope-preprinted", _("Preprinted Envelope") },
      { "envelope-window", _("Windowed Envelope") },
      { "fabric", _("Fabric") },
      { "fabric-archival", _("Archival Fabric") },
      { "fabric-glossy", _("Glossy Fabric") },
      { "fabric-high-gloss", _("High Gloss Fabric") },
      { "fabric-matte", _("Matte Fabric") },
      { "fabric-semi-gloss", _("Semi-Gloss Fabric") },
      { "fabric-waterproof", _("Waterproof Fabric") },
      { "film", _("Film") },
      { "flexo-base", _("Flexo Base") },
      { "flexo-photo-polymer", _("Flexo Photo Polymer") },
      { "flute", _("Flute") },
      { "foil", _("Foil") },
      { "full-cut-tabs", _("Full Cut Tabs") },
      { "glass", _("Glass") },
      { "glass-colored", _("Glass Colored") },
      { "glass-opaque", _("Glass Opaque") },
      { "glass-surfaced", _("Glass Surfaced") },
      { "glass-textured", _("Glass Textured") },
      { "gravure-cylinder", _("Gravure Cylinder") },
      { "image-setter-paper", _("Image Setter Paper") },
      { "imaging-cylinder", _("Imaging Cylinder") },
      { "labels", _("Labels") },
      { "labels-colored", _("Colored Labels") },
      { "labels-glossy", _("Glossy Labels") },
      { "labels-high-gloss", _("High Gloss Labels") },
      { "labels-inkjet", _("Inkjet Labels") },
      { "labels-matte", _("Matte Labels") },
      { "labels-permanent", _("Permanent Labels") },
      { "labels-satin", _("Satin Labels") },
      { "labels-security", _("Security Labels") },
      { "labels-semi-gloss", _("Semi-Gloss Labels") },
      { "laminating-foil", _("Laminating Foil") },
      { "letterhead", _("Letterhead") },
      { "metal", _("Metal") },
      { "metal-glossy", _("Metal Glossy") },
      { "metal-high-gloss", _("Metal High Gloss") },
      { "metal-matte", _("Metal Matte") },
      { "metal-satin", _("Metal Satin") },
      { "metal-semi-gloss", _("Metal Semi Gloss") },
      { "mounting-tape", _("Mounting Tape") },
      { "multi-layer", _("Multi Layer") },
      { "multi-part-form", _("Multi Part Form") },
      { "other", _("Other") },
      { "paper", _("Paper") },
      { "photographic", _("Photo Paper") },
      { "photographic-archival", _("Photographic Archival") },
      { "photographic-film", _("Photo Film") },
      { "photographic-glossy", _("Glossy Photo Paper") },
      { "photographic-high-gloss", _("High Gloss Photo Paper") },
      { "photographic-matte", _("Matte Photo Paper") },
      { "photographic-satin", _("Satin Photo Paper") },
      { "photographic-semi-gloss", _("Semi-Gloss Photo Paper") },
      { "plastic", _("Plastic") },
      { "plastic-archival", _("Plastic Archival") },
      { "plastic-colored", _("Plastic Colored") },
      { "plastic-glossy", _("Plastic Glossy") },
      { "plastic-high-gloss", _("Plastic High Gloss") },
      { "plastic-matte", _("Plastic Matte") },
      { "plastic-satin", _("Plastic Satin") },
      { "plastic-semi-gloss", _("Plastic Semi Gloss") },
      { "plate", _("Plate") },
      { "polyester", _("Polyester") },
      { "pre-cut-tabs", _("Pre Cut Tabs") },
      { "roll", _("Roll") },
      { "screen", _("Screen") },
      { "screen-paged", _("Screen Paged") },
      { "self-adhesive", _("Self Adhesive") },
      { "self-adhesive-film", _("Self Adhesive Film") },
      { "shrink-foil", _("Shrink Foil") },
      { "single-face", _("Single Face") },
      { "single-wall", _("Single Wall Cardboard") },
      { "sleeve", _("Sleeve") },
      { "stationery", _("Stationery") },
      { "stationery-archival", _("Stationery Archival") },
      { "stationery-coated", _("Coated Paper") },
      { "stationery-cotton", _("Stationery Cotton") },
      { "stationery-fine", _("Vellum Paper") },
      { "stationery-heavyweight", _("Heavyweight Paper") },
      { "stationery-heavyweight-coated", _("Stationery Heavyweight Coated") },
      { "stationery-inkjet", _("Stationery Inkjet Paper") },
      { "stationery-letterhead", _("Letterhead") },
      { "stationery-lightweight", _("Lightweight Paper") },
      { "stationery-preprinted", _("Preprinted Paper") },
      { "stationery-prepunched", _("Punched Paper") },
      { "tab-stock", _("Tab Stock") },
      { "tractor", _("Tractor") },
      { "transfer", _("Transfer") },
      { "transparency", _("Transparency") },
      { "triple-wall", _("Triple Wall Cardboard") },
      { "wet-film", _("Wet Film") }
    };

    cupsFilePrintf(fp, "*OpenUI *MediaType: PickOne\n"
                       "*OrderDependency: 10 AnySetup *MediaType\n"
                       "*DefaultMediaType: %s\n", ppdname);
    for (i = 0; i < (int)(sizeof(media_types) / sizeof(media_types[0])); i ++)
    {
      if (!ippContainsString(attr, media_types[i][0]))
        continue;

      pwg_ppdize_name(media_types[i][0], ppdname, sizeof(ppdname));

      cupsFilePrintf(fp, "*MediaType %s/%s: \"<</MediaType(%s)>>setpagedevice\"\n", ppdname, _cupsLangString(lang, media_types[i][1]), ppdname);
    }
    cupsFilePuts(fp, "*CloseUI: *MediaType\n");
  }

 /*
  * ColorModel...
  */

  if ((attr = ippFindAttribute(response, "pwg-raster-document-type-supported", IPP_TAG_KEYWORD)) == NULL)
    if ((attr = ippFindAttribute(response, "urf-supported", IPP_TAG_KEYWORD)) == NULL)
      if ((attr = ippFindAttribute(response, "print-color-mode-supported", IPP_TAG_KEYWORD)) == NULL)
        attr = ippFindAttribute(response, "output-mode-supported", IPP_TAG_KEYWORD);

  if (attr)
  {
    const char *default_color = NULL;	/* Default */

    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      const char *keyword = ippGetString(attr, i, NULL);
					/* Keyword for color/bit depth */

      if (!strcmp(keyword, "black_1") || !strcmp(keyword, "bi-level") || !strcmp(keyword, "process-bi-level"))
      {
        if (!default_color)
	  cupsFilePrintf(fp, "*OpenUI *ColorModel/%s: PickOne\n"
			     "*OrderDependency: 10 AnySetup *ColorModel\n", _cupsLangString(lang, _("Color Mode")));

        cupsFilePrintf(fp, "*ColorModel FastGray/%s: \"<</cupsColorSpace 3/cupsBitsPerColor 1/cupsColorOrder 0/cupsCompression 0>>setpagedevice\"\n", _cupsLangString(lang, _("Fast Grayscale")));

        if (!default_color)
	  default_color = "FastGray";
      }
      else if (!strcmp(keyword, "sgray_8") || !strcmp(keyword, "W8") || !strcmp(keyword, "monochrome") || !strcmp(keyword, "process-monochrome"))
      {
        if (!default_color)
	  cupsFilePrintf(fp, "*OpenUI *ColorModel/%s: PickOne\n"
			     "*OrderDependency: 10 AnySetup *ColorModel\n", _cupsLangString(lang, _("Color Mode")));

        cupsFilePrintf(fp, "*ColorModel Gray/%s: \"<</cupsColorSpace 18/cupsBitsPerColor 8/cupsColorOrder 0/cupsCompression 0>>setpagedevice\"\n", _cupsLangString(lang, _("Grayscale")));

        if (!default_color || !strcmp(default_color, "FastGray"))
	  default_color = "Gray";
      }
      else if (!strcmp(keyword, "srgb_8") || !strcmp(keyword, "SRGB24") || !strcmp(keyword, "color"))
      {
        if (!default_color)
	  cupsFilePrintf(fp, "*OpenUI *ColorModel/%s: PickOne\n"
			     "*OrderDependency: 10 AnySetup *ColorModel\n", _cupsLangString(lang, _("Color Mode")));

        cupsFilePrintf(fp, "*ColorModel RGB/%s: \"<</cupsColorSpace 19/cupsBitsPerColor 8/cupsColorOrder 0/cupsCompression 0>>setpagedevice\"\n", _cupsLangString(lang, _("Color")));

	default_color = "RGB";
      }
      else if (!strcmp(keyword, "adobe-rgb_16") || !strcmp(keyword, "ADOBERGB48"))
      {
        if (!default_color)
	  cupsFilePrintf(fp, "*OpenUI *ColorModel/%s: PickOne\n"
			     "*OrderDependency: 10 AnySetup *ColorModel\n", _cupsLangString(lang, _("Color Mode")));

        cupsFilePrintf(fp, "*ColorModel AdobeRGB/%s: \"<</cupsColorSpace 20/cupsBitsPerColor 16/cupsColorOrder 0/cupsCompression 0>>setpagedevice\"\n", _cupsLangString(lang, _("Deep Color")));

        if (!default_color)
	  default_color = "AdobeRGB";
      }
    }

    if (default_color)
    {
      cupsFilePrintf(fp, "*DefaultColorModel: %s\n", default_color);
      cupsFilePuts(fp, "*CloseUI: *ColorModel\n");
    }
  }

 /*
  * Duplex...
  */

  if ((attr = ippFindAttribute(response, "sides-supported", IPP_TAG_KEYWORD)) != NULL && ippContainsString(attr, "two-sided-long-edge"))
  {
    cupsFilePrintf(fp, "*OpenUI *Duplex/%s: PickOne\n"
		       "*OrderDependency: 10 AnySetup *Duplex\n"
		       "*DefaultDuplex: None\n"
		       "*Duplex None/%s: \"<</Duplex false>>setpagedevice\"\n"
		       "*Duplex DuplexNoTumble/%s: \"<</Duplex true/Tumble false>>setpagedevice\"\n"
		       "*Duplex DuplexTumble/%s: \"<</Duplex true/Tumble true>>setpagedevice\"\n"
		       "*CloseUI: *Duplex\n", _cupsLangString(lang, _("2-Sided Printing")), _cupsLangString(lang, _("Off (1-Sided)")), _cupsLangString(lang, _("Long-Edge (Portrait)")), _cupsLangString(lang, _("Short-Edge (Landscape)")));

    if ((attr = ippFindAttribute(response, "pwg-raster-document-sheet-back", IPP_TAG_KEYWORD)) != NULL)
    {
      const char *keyword = ippGetString(attr, 0, NULL);
					/* Keyword value */

      if (!strcmp(keyword, "flipped"))
        cupsFilePuts(fp, "*cupsBackSide: Flipped\n");
      else if (!strcmp(keyword, "manual-tumble"))
        cupsFilePuts(fp, "*cupsBackSide: ManualTumble\n");
      else if (!strcmp(keyword, "normal"))
        cupsFilePuts(fp, "*cupsBackSide: Normal\n");
      else
        cupsFilePuts(fp, "*cupsBackSide: Rotated\n");
    }
    else if ((attr = ippFindAttribute(response, "urf-supported", IPP_TAG_KEYWORD)) != NULL)
    {
      for (i = 0, count = ippGetCount(attr); i < count; i ++)
      {
	const char *dm = ippGetString(attr, i, NULL);
					  /* DM value */

	if (!_cups_strcasecmp(dm, "DM1"))
	{
	  cupsFilePuts(fp, "*cupsBackSide: Normal\n");
	  break;
	}
	else if (!_cups_strcasecmp(dm, "DM2"))
	{
	  cupsFilePuts(fp, "*cupsBackSide: Flipped\n");
	  break;
	}
	else if (!_cups_strcasecmp(dm, "DM3"))
	{
	  cupsFilePuts(fp, "*cupsBackSide: Rotated\n");
	  break;
	}
	else if (!_cups_strcasecmp(dm, "DM4"))
	{
	  cupsFilePuts(fp, "*cupsBackSide: ManualTumble\n");
	  break;
	}
      }
    }
  }

 /*
  * Finishing options...
  */

  if ((attr = ippFindAttribute(response, "finishings-col-database", IPP_TAG_BEGIN_COLLECTION)) != NULL)
  {
    ipp_t		*col;		/* Collection value */
    ipp_attribute_t	*template;	/* "finishing-template" member */
    const char		*name;		/* String name */
    int			value;		/* Enum value, if any */
    cups_array_t	*names;		/* Names we've added */

    count = ippGetCount(attr);
    names = cupsArrayNew3((cups_array_func_t)strcmp, NULL, NULL, 0, (cups_acopy_func_t)strdup, (cups_afree_func_t)free);

    cupsFilePrintf(fp, "*OpenUI *cupsFinishingTemplate/%s: PickMany\n"
		       "*OrderDependency: 10 AnySetup *cupsFinishingTemplate\n"
		       "*DefaultcupsFinishingTemplate: none\n"
		       "*cupsFinishingTemplate none/%s: \"\"\n"
		       "*cupsIPPFinishings 3/none: \"*cupsFinishingTemplate none\"\n", _cupsLangString(lang, _("Finishing")), _cupsLangString(lang, _("No Finishing")));

    for (i = 0; i < count; i ++)
    {
      col      = ippGetCollection(attr, i);
      template = ippFindAttribute(col, "finishing-template", IPP_TAG_ZERO);

      if ((name = ippGetString(template, 0, NULL)) == NULL || !strcmp(name, "none"))
        continue;

      if (cupsArrayFind(names, (char *)name))
        continue;			/* Already did this finishing template */

      cupsArrayAdd(names, (char *)name);

      for (j = 0; j < (int)(sizeof(finishings) / sizeof(finishings[0])); j ++)
      {
        if (!strcmp(finishings[j][0], name))
	{
          cupsFilePrintf(fp, "*cupsFinishingTemplate %s/%s: \"\"\n", name, _cupsLangString(lang, finishings[j][1]));

	  value = ippEnumValue("finishings", name);

	  if (value)
	    cupsFilePrintf(fp, "*cupsIPPFinishings %d/%s: \"*cupsFinishingTemplate %s\"\n", value, name, name);
          break;
	}
      }
    }

    cupsArrayDelete(names);

    cupsFilePuts(fp, "*CloseUI: *cupsFinishingTemplate\n");
  }
  else if ((attr = ippFindAttribute(response, "finishings-supported", IPP_TAG_ENUM)) != NULL && (count = ippGetCount(attr)) > 1 )
  {
    const char		*name;		/* String name */
    int			value;		/* Enum value, if any */

    count = ippGetCount(attr);

    cupsFilePrintf(fp, "*OpenUI *cupsFinishingTemplate/%s: PickMany\n"
		       "*OrderDependency: 10 AnySetup *cupsFinishingTemplate\n"
		       "*DefaultcupsFinishingTemplate: none\n"
		       "*cupsFinishingTemplate none/%s: \"\"\n"
		       "*cupsIPPFinishings 3/none: \"*cupsFinishingTemplate none\"\n", _cupsLangString(lang, _("Finishing")), _cupsLangString(lang, _("No Finishing")));

    for (i = 0; i < count; i ++)
    {
      if ((value = ippGetInteger(attr, i)) == 3)
        continue;

      name = ippEnumString("finishings", value);
      for (j = 0; j < (int)(sizeof(finishings) / sizeof(finishings[0])); j ++)
      {
        if (!strcmp(finishings[j][0], name))
	{
          cupsFilePrintf(fp, "*cupsFinishingTemplate %s/%s: \"\"\n", name, _cupsLangString(lang, finishings[j][1]));
	  cupsFilePrintf(fp, "*cupsIPPFinishings %d/%s: \"*cupsFinishingTemplate %s\"\n", value, name, name);
          break;
	}
      }
    }

    cupsFilePuts(fp, "*CloseUI: *cupsFinishingTemplate\n");
  }

 /*
  * cupsPrintQuality and DefaultResolution...
  */

  if ((attr = ippFindAttribute(response, "pwg-raster-document-resolution-supported", IPP_TAG_RESOLUTION)) != NULL)
  {
    count = ippGetCount(attr);

    pwg_ppdize_resolution(attr, count / 2, &xres, &yres, ppdname, sizeof(ppdname));
    cupsFilePrintf(fp, "*DefaultResolution: %s\n", ppdname);

    cupsFilePrintf(fp, "*OpenUI *cupsPrintQuality/%s: PickOne\n"
		       "*OrderDependency: 10 AnySetup *cupsPrintQuality\n"
		       "*DefaultcupsPrintQuality: Normal\n", _cupsLangString(lang, _("Print Quality")));
    if (count > 2)
    {
      pwg_ppdize_resolution(attr, 0, &xres, &yres, NULL, 0);
      cupsFilePrintf(fp, "*cupsPrintQuality Draft/%s: \"<</HWResolution[%d %d]>>setpagedevice\"\n", _cupsLangString(lang, _("Draft")), xres, yres);
    }
    pwg_ppdize_resolution(attr, count / 2, &xres, &yres, NULL, 0);
    cupsFilePrintf(fp, "*cupsPrintQuality Normal/%s: \"<</HWResolution[%d %d]>>setpagedevice\"\n", _cupsLangString(lang, _("Normal")), xres, yres);
    if (count > 1)
    {
      pwg_ppdize_resolution(attr, count - 1, &xres, &yres, NULL, 0);
      cupsFilePrintf(fp, "*cupsPrintQuality High/%s: \"<</HWResolution[%d %d]>>setpagedevice\"\n", _cupsLangString(lang, _("High")), xres, yres);
    }

    cupsFilePuts(fp, "*CloseUI: *cupsPrintQuality\n");
  }
  else if ((attr = ippFindAttribute(response, "urf-supported", IPP_TAG_KEYWORD)) != NULL)
  {
    int lowdpi = 0, hidpi = 0;		/* Lower and higher resolution */

    for (i = 0, count = ippGetCount(attr); i < count; i ++)
    {
      const char *rs = ippGetString(attr, i, NULL);
					/* RS value */

      if (_cups_strncasecmp(rs, "RS", 2))
        continue;

      lowdpi = atoi(rs + 2);
      if ((rs = strrchr(rs, '-')) != NULL)
        hidpi = atoi(rs + 1);
      else
        hidpi = lowdpi;
      break;
    }

    if (lowdpi == 0)
    {
     /*
      * Invalid "urf-supported" value...
      */

      goto bad_ppd;
    }
    else
    {
     /*
      * Generate print qualities based on low and high DPIs...
      */

      cupsFilePrintf(fp, "*DefaultResolution: %ddpi\n", lowdpi);

      cupsFilePrintf(fp, "*OpenUI *cupsPrintQuality/%s: PickOne\n"
			 "*OrderDependency: 10 AnySetup *cupsPrintQuality\n"
			 "*DefaultcupsPrintQuality: Normal\n", _cupsLangString(lang, _("Print Quality")));
      if ((lowdpi & 1) == 0)
	cupsFilePrintf(fp, "*cupsPrintQuality Draft/%s: \"<</HWResolution[%d %d]>>setpagedevice\"\n", _cupsLangString(lang, _("Draft")), lowdpi, lowdpi / 2);
      cupsFilePrintf(fp, "*cupsPrintQuality Normal/%s: \"<</HWResolution[%d %d]>>setpagedevice\"\n", _cupsLangString(lang, _("Normal")), lowdpi, lowdpi);
      if (hidpi > lowdpi)
	cupsFilePrintf(fp, "*cupsPrintQuality High/%s: \"<</HWResolution[%d %d]>>setpagedevice\"\n", _cupsLangString(lang, _("High")), hidpi, hidpi);
      cupsFilePuts(fp, "*CloseUI: *cupsPrintQuality\n");
    }
  }
  else if (is_apple || is_pwg)
    goto bad_ppd;
  else if ((attr = ippFindAttribute(response, "printer-resolution-default", IPP_TAG_RESOLUTION)) != NULL)
  {
    pwg_ppdize_resolution(attr, 0, &xres, &yres, ppdname, sizeof(ppdname));
    cupsFilePrintf(fp, "*DefaultResolution: %s\n", ppdname);
  }
  else
    cupsFilePuts(fp, "*DefaultResolution: 300dpi\n");

 /*
  * Close up and return...
  */

  cupsFileClose(fp);

  return (buffer);

 /*
  * If we get here then there was a problem creating the PPD...
  */

  bad_ppd:

  cupsFileClose(fp);
  unlink(buffer);
  *buffer = '\0';

  return (NULL);
}