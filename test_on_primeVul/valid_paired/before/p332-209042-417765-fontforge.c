bool SFD_GetFontMetaData( FILE *sfd,
			  char *tok,
			  SplineFont *sf,
			  SFD_GetFontMetaDataData* d )
{
    int ch;
    int i;
    KernClass* kc = 0;
    int old;
    char val[2000];

    // This allows us to assume we can dereference d
    // at all times
    static SFD_GetFontMetaDataData my_static_d;
    static int my_static_d_is_virgin = 1;
    if( !d )
    {
	if( my_static_d_is_virgin )
	{
	    my_static_d_is_virgin = 0;
	    SFD_GetFontMetaDataData_Init( &my_static_d );
	}
	d = &my_static_d;
    }

    if ( strmatch(tok,"FontName:")==0 )
    {
	geteol(sfd,val);
	sf->fontname = copy(val);
    }
    else if ( strmatch(tok,"FullName:")==0 )
    {
	geteol(sfd,val);
	sf->fullname = copy(val);
    }
    else if ( strmatch(tok,"FamilyName:")==0 )
    {
	geteol(sfd,val);
	sf->familyname = copy(val);
    }
    else if ( strmatch(tok,"DefaultBaseFilename:")==0 )
    {
	geteol(sfd,val);
	sf->defbasefilename = copy(val);
    }
    else if ( strmatch(tok,"Weight:")==0 )
    {
	getprotectedname(sfd,val);
	sf->weight = copy(val);
    }
    else if ( strmatch(tok,"Copyright:")==0 )
    {
	sf->copyright = getquotedeol(sfd);
    }
    else if ( strmatch(tok,"Comments:")==0 )
    {
	char *temp = getquotedeol(sfd);
	sf->comments = latin1_2_utf8_copy(temp);
	free(temp);
    }
    else if ( strmatch(tok,"UComments:")==0 )
    {
	sf->comments = SFDReadUTF7Str(sfd);
    }
    else if ( strmatch(tok,"FontLog:")==0 )
    {
	sf->fontlog = SFDReadUTF7Str(sfd);
    }
    else if ( strmatch(tok,"Version:")==0 )
    {
	geteol(sfd,val);
	sf->version = copy(val);
    }
    else if ( strmatch(tok,"StyleMapFamilyName:")==0 )
    {
    sf->styleMapFamilyName = SFDReadUTF7Str(sfd);
    }
    /* Legacy attribute for StyleMapFamilyName. Deprecated. */
    else if ( strmatch(tok,"OS2FamilyName:")==0 )
    {
    if (sf->styleMapFamilyName == NULL)
        sf->styleMapFamilyName = SFDReadUTF7Str(sfd);
    }
    else if ( strmatch(tok,"FONDName:")==0 )
    {
	geteol(sfd,val);
	sf->fondname = copy(val);
    }
    else if ( strmatch(tok,"ItalicAngle:")==0 )
    {
	getreal(sfd,&sf->italicangle);
    }
    else if ( strmatch(tok,"StrokeWidth:")==0 )
    {
	getreal(sfd,&sf->strokewidth);
    }
    else if ( strmatch(tok,"UnderlinePosition:")==0 )
    {
	getreal(sfd,&sf->upos);
    }
    else if ( strmatch(tok,"UnderlineWidth:")==0 )
    {
	getreal(sfd,&sf->uwidth);
    }
    else if ( strmatch(tok,"ModificationTime:")==0 )
    {
	getlonglong(sfd,&sf->modificationtime);
    }
    else if ( strmatch(tok,"CreationTime:")==0 )
    {
	getlonglong(sfd,&sf->creationtime);
	d->hadtimes = true;
    }
    else if ( strmatch(tok,"PfmFamily:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->pfminfo.pfmfamily = temp;
	sf->pfminfo.pfmset = true;
    }
    else if ( strmatch(tok,"LangName:")==0 )
    {
	sf->names = SFDGetLangName(sfd,sf->names);
    }
    else if ( strmatch(tok,"GaspTable:")==0 )
    {
	SFDGetGasp(sfd,sf);
    }
    else if ( strmatch(tok,"DesignSize:")==0 )
    {
	SFDGetDesignSize(sfd,sf);
    }
    else if ( strmatch(tok,"OtfFeatName:")==0 )
    {
	SFDGetOtfFeatName(sfd,sf);
    }
    else if ( strmatch(tok,"PfmWeight:")==0 || strmatch(tok,"TTFWeight:")==0 )
    {
	getsint(sfd,&sf->pfminfo.weight);
	sf->pfminfo.pfmset = true;
    }
    else if ( strmatch(tok,"TTFWidth:")==0 )
    {
	getsint(sfd,&sf->pfminfo.width);
	sf->pfminfo.pfmset = true;
    }
    else if ( strmatch(tok,"Panose:")==0 )
    {
	int temp,i;
	for ( i=0; i<10; ++i )
	{
	    getint(sfd,&temp);
	    sf->pfminfo.panose[i] = temp;
	}
	sf->pfminfo.panose_set = true;
    }
    else if ( strmatch(tok,"LineGap:")==0 )
    {
	getsint(sfd,&sf->pfminfo.linegap);
	sf->pfminfo.pfmset = true;
    }
    else if ( strmatch(tok,"VLineGap:")==0 )
    {
	getsint(sfd,&sf->pfminfo.vlinegap);
	sf->pfminfo.pfmset = true;
    }
    else if ( strmatch(tok,"HheadAscent:")==0 )
    {
	getsint(sfd,&sf->pfminfo.hhead_ascent);
    }
    else if ( strmatch(tok,"HheadAOffset:")==0 )
    {
	int temp;
	getint(sfd,&temp); sf->pfminfo.hheadascent_add = temp;
    }
    else if ( strmatch(tok,"HheadDescent:")==0 )
    {
	getsint(sfd,&sf->pfminfo.hhead_descent);
    }
    else if ( strmatch(tok,"HheadDOffset:")==0 )
    {
	int temp;
	getint(sfd,&temp); sf->pfminfo.hheaddescent_add = temp;
    }
    else if ( strmatch(tok,"OS2TypoLinegap:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_typolinegap);
    }
    else if ( strmatch(tok,"OS2TypoAscent:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_typoascent);
    }
    else if ( strmatch(tok,"OS2TypoAOffset:")==0 )
    {
	int temp;
	getint(sfd,&temp); sf->pfminfo.typoascent_add = temp;
    }
    else if ( strmatch(tok,"OS2TypoDescent:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_typodescent);
    }
    else if ( strmatch(tok,"OS2TypoDOffset:")==0 )
    {
	int temp;
	getint(sfd,&temp); sf->pfminfo.typodescent_add = temp;
    }
    else if ( strmatch(tok,"OS2WinAscent:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_winascent);
    }
    else if ( strmatch(tok,"OS2WinDescent:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_windescent);
    }
    else if ( strmatch(tok,"OS2WinAOffset:")==0 )
    {
	int temp;
	getint(sfd,&temp); sf->pfminfo.winascent_add = temp;
    }
    else if ( strmatch(tok,"OS2WinDOffset:")==0 )
    {
	int temp;
	getint(sfd,&temp); sf->pfminfo.windescent_add = temp;
    }
    else if ( strmatch(tok,"HHeadAscent:")==0 )
    {
	// DUPLICATE OF ABOVE
	getsint(sfd,&sf->pfminfo.hhead_ascent);
    }
    else if ( strmatch(tok,"HHeadDescent:")==0 )
    {
	// DUPLICATE OF ABOVE
	getsint(sfd,&sf->pfminfo.hhead_descent);
    }

    else if ( strmatch(tok,"HHeadAOffset:")==0 )
    {
	// DUPLICATE OF ABOVE
	int temp;
	getint(sfd,&temp); sf->pfminfo.hheadascent_add = temp;
    }
    else if ( strmatch(tok,"HHeadDOffset:")==0 )
    {
	// DUPLICATE OF ABOVE
	int temp;
	getint(sfd,&temp); sf->pfminfo.hheaddescent_add = temp;
    }
    else if ( strmatch(tok,"MacStyle:")==0 )
    {
	getsint(sfd,&sf->macstyle);
    }
    else if ( strmatch(tok,"OS2SubXSize:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_subxsize);
	sf->pfminfo.subsuper_set = true;
    }
    else if ( strmatch(tok,"OS2SubYSize:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_subysize);
    }
    else if ( strmatch(tok,"OS2SubXOff:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_subxoff);
    }
    else if ( strmatch(tok,"OS2SubYOff:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_subyoff);
    }
    else if ( strmatch(tok,"OS2SupXSize:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_supxsize);
    }
    else if ( strmatch(tok,"OS2SupYSize:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_supysize);
    }
    else if ( strmatch(tok,"OS2SupXOff:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_supxoff);
    }
    else if ( strmatch(tok,"OS2SupYOff:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_supyoff);
    }
    else if ( strmatch(tok,"OS2StrikeYSize:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_strikeysize);
    }
    else if ( strmatch(tok,"OS2StrikeYPos:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_strikeypos);
    }
    else if ( strmatch(tok,"OS2CapHeight:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_capheight);
    }
    else if ( strmatch(tok,"OS2XHeight:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_xheight);
    }
    else if ( strmatch(tok,"OS2FamilyClass:")==0 )
    {
	getsint(sfd,&sf->pfminfo.os2_family_class);
    }
    else if ( strmatch(tok,"OS2Vendor:")==0 )
    {
	while ( isspace(nlgetc(sfd)));
	sf->pfminfo.os2_vendor[0] = nlgetc(sfd);
	sf->pfminfo.os2_vendor[1] = nlgetc(sfd);
	sf->pfminfo.os2_vendor[2] = nlgetc(sfd);
	sf->pfminfo.os2_vendor[3] = nlgetc(sfd);
	(void) nlgetc(sfd);
    }
    else if ( strmatch(tok,"OS2CodePages:")==0 )
    {
	gethexints(sfd,sf->pfminfo.codepages,2);
	sf->pfminfo.hascodepages = true;
    }
    else if ( strmatch(tok,"OS2UnicodeRanges:")==0 )
    {
	gethexints(sfd,sf->pfminfo.unicoderanges,4);
	sf->pfminfo.hasunicoderanges = true;
    }
    else if ( strmatch(tok,"TopEncoding:")==0 )
    {
	/* Obsolete */
	getint(sfd,&sf->top_enc);
    }
    else if ( strmatch(tok,"Ascent:")==0 )
    {
	getint(sfd,&sf->ascent);
    }
    else if ( strmatch(tok,"Descent:")==0 )
    {
	getint(sfd,&sf->descent);
    }
    else if ( strmatch(tok,"InvalidEm:")==0 )
    {
	getint(sfd,&sf->invalidem);
    }
    else if ( strmatch(tok,"woffMajor:")==0 )
    {
	getint(sfd,&sf->woffMajor);
    }
    else if ( strmatch(tok,"woffMinor:")==0 )
    {
	getint(sfd,&sf->woffMinor);
    }
    else if ( strmatch(tok,"woffMetadata:")==0 )
    {
	sf->woffMetadata = SFDReadUTF7Str(sfd);
    }
    else if ( strmatch(tok,"UFOAscent:")==0 )
    {
	    getreal(sfd,&sf->ufo_ascent);
    }
    else if ( strmatch(tok,"UFODescent:")==0 )
    {
	getreal(sfd,&sf->ufo_descent);
    }
    else if ( strmatch(tok,"sfntRevision:")==0 )
    {
	gethex(sfd,(uint32 *)&sf->sfntRevision);
    }
    else if ( strmatch(tok,"LayerCount:")==0 )
    {
	d->had_layer_cnt = true;
	getint(sfd,&sf->layer_cnt);
	if ( sf->layer_cnt>2 ) {
	    sf->layers = realloc(sf->layers,sf->layer_cnt*sizeof(LayerInfo));
	    memset(sf->layers+2,0,(sf->layer_cnt-2)*sizeof(LayerInfo));
	}
    }
    else if ( strmatch(tok,"Layer:")==0 )
    {
        // TODO: Read the U. F. O. path.
	int layer, o2, bk;
	getint(sfd,&layer);
	if ( layer>=sf->layer_cnt ) {
	    sf->layers = realloc(sf->layers,(layer+1)*sizeof(LayerInfo));
	    memset(sf->layers+sf->layer_cnt,0,((layer+1)-sf->layer_cnt)*sizeof(LayerInfo));
	    sf->layer_cnt = layer+1;
	}
	getint(sfd,&o2);
	sf->layers[layer].order2 = o2;
	sf->layers[layer].background = layer==ly_back;
	/* Used briefly, now background is after layer name */
	while ( (ch=nlgetc(sfd))==' ' );
	ungetc(ch,sfd);
	if ( ch!='"' ) {
	    getint(sfd,&bk);
	    sf->layers[layer].background = bk;
	}
	/* end of section for obsolete format */
	sf->layers[layer].name = SFDReadUTF7Str(sfd);
	while ( (ch=nlgetc(sfd))==' ' );
	ungetc(ch,sfd);
	if ( ch!='\n' ) {
	    getint(sfd,&bk);
	    sf->layers[layer].background = bk;
	}
	while ( (ch=nlgetc(sfd))==' ' );
	ungetc(ch,sfd);
	if ( ch!='\n' ) { sf->layers[layer].ufo_path = SFDReadUTF7Str(sfd); }
    }
    else if ( strmatch(tok,"PreferredKerning:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->preferred_kerning = temp;
    }
    else if ( strmatch(tok,"StrokedFont:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->strokedfont = temp;
    }
    else if ( strmatch(tok,"MultiLayer:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->multilayer = temp;
    }
    else if ( strmatch(tok,"NeedsXUIDChange:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->changed_since_xuidchanged = temp;
    }
    else if ( strmatch(tok,"VerticalOrigin:")==0 )
    {
	// this doesn't seem to be written ever.
	int temp;
	getint(sfd,&temp);
	sf->hasvmetrics = true;
    }
    else if ( strmatch(tok,"HasVMetrics:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->hasvmetrics = temp;
    }
    else if ( strmatch(tok,"Justify:")==0 )
    {
	SFDParseJustify(sfd,sf,tok);
    }
    else if ( strmatch(tok,"BaseHoriz:")==0 )
    {
	sf->horiz_base = SFDParseBase(sfd);
	d->last_base = sf->horiz_base;
	d->last_base_script = NULL;
    }
    else if ( strmatch(tok,"BaseVert:")==0 )
    {
	sf->vert_base = SFDParseBase(sfd);
	d->last_base = sf->vert_base;
	d->last_base_script = NULL;
    }
    else if ( strmatch(tok,"BaseScript:")==0 )
    {
	struct basescript *bs = SFDParseBaseScript(sfd,d->last_base);
	if ( d->last_base==NULL )
	{
	    BaseScriptFree(bs);
	    bs = NULL;
	}
	else if ( d->last_base_script!=NULL )
	    d->last_base_script->next = bs;
	else
	    d->last_base->scripts = bs;
	d->last_base_script = bs;
    }
    else if ( strmatch(tok,"StyleMap:")==0 )
    {
    gethex(sfd,(uint32 *)&sf->pfminfo.stylemap);
    }
    /* Legacy attribute for StyleMap. Deprecated. */
    else if ( strmatch(tok,"OS2StyleName:")==0 )
    {
    char* sname = SFDReadUTF7Str(sfd);
    if (sf->pfminfo.stylemap == -1) {
        if (strcmp(sname,"bold italic")==0) sf->pfminfo.stylemap = 0x21;
        else if (strcmp(sname,"bold")==0) sf->pfminfo.stylemap = 0x20;
        else if (strcmp(sname,"italic")==0) sf->pfminfo.stylemap = 0x01;
        else if (strcmp(sname,"regular")==0) sf->pfminfo.stylemap = 0x40;
    }
    free(sname);
    }
    else if ( strmatch(tok,"FSType:")==0 )
    {
	getsint(sfd,&sf->pfminfo.fstype);
    }
    else if ( strmatch(tok,"OS2Version:")==0 )
    {
	getsint(sfd,&sf->os2_version);
    }
    else if ( strmatch(tok,"OS2_WeightWidthSlopeOnly:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->weight_width_slope_only = temp;
    }
    else if ( strmatch(tok,"OS2_UseTypoMetrics:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->use_typo_metrics = temp;
    }
    else if ( strmatch(tok,"UseUniqueID:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->use_uniqueid = temp;
    }
    else if ( strmatch(tok,"UseXUID:")==0 )
    {
	int temp;
	getint(sfd,&temp);
	sf->use_xuid = temp;
    }
    else if ( strmatch(tok,"UniqueID:")==0 )
    {
	getint(sfd,&sf->uniqueid);
    }
    else if ( strmatch(tok,"XUID:")==0 )
    {
	geteol(sfd,tok);
	sf->xuid = copy(tok);
    }
    else if ( strmatch(tok,"Lookup:")==0 )
    {
	OTLookup *otl;
	int temp;
	if ( sf->sfd_version<2 ) {
	    IError( "Lookups should not happen in version 1 sfd files." );
	    exit(1);
	}
	otl = chunkalloc(sizeof(OTLookup));
	getint(sfd,&temp); otl->lookup_type = temp;
	getint(sfd,&temp); otl->lookup_flags = temp;
	getint(sfd,&temp); otl->store_in_afm = temp;
	otl->lookup_name = SFDReadUTF7Str(sfd);
	if ( otl->lookup_type<gpos_single ) {
	    if ( d->lastsotl==NULL )
		sf->gsub_lookups = otl;
	    else
		d->lastsotl->next = otl;
	    d->lastsotl = otl;
	} else {
	    if ( d->lastpotl==NULL )
		sf->gpos_lookups = otl;
	    else
		d->lastpotl->next = otl;
	    d->lastpotl = otl;
	}
	SFDParseLookup(sfd,otl);
    }
    else if ( strmatch(tok,"MarkAttachClasses:")==0 )
    {
	getint(sfd,&sf->mark_class_cnt);
	sf->mark_classes = malloc(sf->mark_class_cnt*sizeof(char *));
	sf->mark_class_names = malloc(sf->mark_class_cnt*sizeof(char *));
	sf->mark_classes[0] = NULL; sf->mark_class_names[0] = NULL;
	for ( i=1; i<sf->mark_class_cnt; ++i )
	{
	    /* Class 0 is unused */
	    int temp;
	    while ( (temp=nlgetc(sfd))=='\n' || temp=='\r' ); ungetc(temp,sfd);
	    sf->mark_class_names[i] = SFDReadUTF7Str(sfd);
	    getint(sfd,&temp);
	    sf->mark_classes[i] = malloc(temp+1); sf->mark_classes[i][temp] = '\0';
	    nlgetc(sfd);	/* skip space */
	    fread(sf->mark_classes[i],1,temp,sfd);
	}
    }
    else if ( strmatch(tok,"MarkAttachSets:")==0 )
    {
	getint(sfd,&sf->mark_set_cnt);
	sf->mark_sets = malloc(sf->mark_set_cnt*sizeof(char *));
	sf->mark_set_names = malloc(sf->mark_set_cnt*sizeof(char *));
	for ( i=0; i<sf->mark_set_cnt; ++i )
	{
	    /* Set 0 is used */
	    int temp;
	    while ( (temp=nlgetc(sfd))=='\n' || temp=='\r' ); ungetc(temp,sfd);
	    sf->mark_set_names[i] = SFDReadUTF7Str(sfd);
	    getint(sfd,&temp);
	    sf->mark_sets[i] = malloc(temp+1); sf->mark_sets[i][temp] = '\0';
	    nlgetc(sfd);	/* skip space */
	    fread(sf->mark_sets[i],1,temp,sfd);
	}
    }
    else if ( strmatch(tok,"KernClass2:")==0 || strmatch(tok,"VKernClass2:")==0 ||
	      strmatch(tok,"KernClass:")==0 || strmatch(tok,"VKernClass:")==0 ||
	      strmatch(tok,"KernClass3:")==0 || strmatch(tok,"VKernClass3:")==0 )
    {
	int kernclassversion = 0;
	int isv = tok[0]=='V';
	int kcvoffset = (isv ? 10 : 9); //Offset to read kerning class version
	if (isdigit(tok[kcvoffset])) kernclassversion = tok[kcvoffset] - '0';
	int temp, classstart=1;
	int old = (kernclassversion == 0);

	if ( (sf->sfd_version<2)!=old ) {
	    IError( "Version mixup in Kerning Classes of sfd file." );
	    exit(1);
	}
	kc = chunkalloc(old ? sizeof(KernClass1) : sizeof(KernClass));
	getint(sfd,&kc->first_cnt);
	ch=nlgetc(sfd);
	if ( ch=='+' )
	    classstart = 0;
	else
	    ungetc(ch,sfd);
	getint(sfd,&kc->second_cnt);
	if ( old ) {
	    getint(sfd,&temp); ((KernClass1 *) kc)->sli = temp;
	    getint(sfd,&temp); ((KernClass1 *) kc)->flags = temp;
	} else {
	    kc->subtable = SFFindLookupSubtableAndFreeName(sf,SFDReadUTF7Str(sfd));
	    if ( kc->subtable!=NULL && kc->subtable->kc==NULL )
		kc->subtable->kc = kc;
	    else {
		if ( kc->subtable==NULL )
		    LogError(_("Bad SFD file, missing subtable in kernclass defn.\n") );
		else
		    LogError(_("Bad SFD file, two kerning classes assigned to the same subtable: %s\n"), kc->subtable->subtable_name );
		kc->subtable = NULL;
	    }
	}
	kc->firsts = calloc(kc->first_cnt,sizeof(char *));
	kc->seconds = calloc(kc->second_cnt,sizeof(char *));
	kc->offsets = calloc(kc->first_cnt*kc->second_cnt,sizeof(int16));
	kc->adjusts = calloc(kc->first_cnt*kc->second_cnt,sizeof(DeviceTable));
	if (kernclassversion >= 3) {
	  kc->firsts_flags = calloc(kc->first_cnt, sizeof(int));
	  kc->seconds_flags = calloc(kc->second_cnt, sizeof(int));
	  kc->offsets_flags = calloc(kc->first_cnt*kc->second_cnt, sizeof(int));
	  kc->firsts_names = calloc(kc->first_cnt, sizeof(char*));
	  kc->seconds_names = calloc(kc->second_cnt, sizeof(char*));
	}
	kc->firsts[0] = NULL;
	for ( i=classstart; i<kc->first_cnt; ++i ) {
	  if (kernclassversion < 3) {
	    getint(sfd,&temp);
	    kc->firsts[i] = malloc(temp+1); kc->firsts[i][temp] = '\0';
	    nlgetc(sfd);	/* skip space */
	    fread(kc->firsts[i],1,temp,sfd);
	  } else {
	    getint(sfd,&kc->firsts_flags[i]);
	    while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd); if (ch == '\n' || ch == EOF) continue;
	    kc->firsts_names[i] = SFDReadUTF7Str(sfd);
	    while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd); if (ch == '\n' || ch == EOF) continue;
	    kc->firsts[i] = SFDReadUTF7Str(sfd);
            if (kc->firsts[i] == NULL) kc->firsts[i] = copy(""); // In certain places, this must be defined.
	    while ((ch=nlgetc(sfd)) == ' ' || ch == '\n'); ungetc(ch, sfd);
	  }
	}
	kc->seconds[0] = NULL;
	for ( i=1; i<kc->second_cnt; ++i ) {
	  if (kernclassversion < 3) {
	    getint(sfd,&temp);
	    kc->seconds[i] = malloc(temp+1); kc->seconds[i][temp] = '\0';
	    nlgetc(sfd);	/* skip space */
	    fread(kc->seconds[i],1,temp,sfd);
	  } else {
	    getint(sfd,&temp);
	    kc->seconds_flags[i] = temp;
	    while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd); if (ch == '\n' || ch == EOF) continue;
	    kc->seconds_names[i] = SFDReadUTF7Str(sfd);
	    while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd); if (ch == '\n' || ch == EOF) continue;
	    kc->seconds[i] = SFDReadUTF7Str(sfd);
            if (kc->seconds[i] == NULL) kc->seconds[i] = copy(""); // In certain places, this must be defined.
	    while ((ch=nlgetc(sfd)) == ' ' || ch == '\n'); ungetc(ch, sfd);
	  }
	}
	for ( i=0; i<kc->first_cnt*kc->second_cnt; ++i ) {
	  if (kernclassversion >= 3) {
	    getint(sfd,&temp);
	    kc->offsets_flags[i] = temp;
	  }
	    getint(sfd,&temp);
	    kc->offsets[i] = temp;
	    SFDReadDeviceTable(sfd,&kc->adjusts[i]);
	}
	if ( !old && kc->subtable == NULL ) {
	    /* Error. Ignore it. Free it. Whatever */;
	} else if ( !isv ) {
	    if ( d->lastkc==NULL )
		sf->kerns = kc;
	    else
		d->lastkc->next = kc;
	    d->lastkc = kc;
	} else {
	    if ( d->lastvkc==NULL )
		sf->vkerns = kc;
	    else
		d->lastvkc->next = kc;
	    d->lastvkc = kc;
	}
    }
    else if ( strmatch(tok,"ContextPos2:")==0 || strmatch(tok,"ContextSub2:")==0 ||
	      strmatch(tok,"ChainPos2:")==0 || strmatch(tok,"ChainSub2:")==0 ||
	      strmatch(tok,"ReverseChain2:")==0 ||
	      strmatch(tok,"ContextPos:")==0 || strmatch(tok,"ContextSub:")==0 ||
	      strmatch(tok,"ChainPos:")==0 || strmatch(tok,"ChainSub:")==0 ||
	      strmatch(tok,"ReverseChain:")==0 )
    {
	FPST *fpst;
	int old;
	if ( strchr(tok,'2')!=NULL ) {
	    old = false;
	    fpst = chunkalloc(sizeof(FPST));
	} else {
	    old = true;
	    fpst = chunkalloc(sizeof(FPST1));
	}
	if ( (sf->sfd_version<2)!=old ) {
	    IError( "Version mixup in FPST of sfd file." );
	    exit(1);
	}
	if ( d->lastfp==NULL )
	    sf->possub = fpst;
	else
	    d->lastfp->next = fpst;
	d->lastfp = fpst;
	SFDParseChainContext(sfd,sf,fpst,tok,old);
    }
    else if ( strmatch(tok,"Group:")==0 ) {
        struct ff_glyphclasses *grouptmp = calloc(1, sizeof(struct ff_glyphclasses));
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        grouptmp->classname = SFDReadUTF7Str(sfd);
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        grouptmp->glyphs = SFDReadUTF7Str(sfd);
        while ((ch=nlgetc(sfd)) == ' ' || ch == '\n'); ungetc(ch, sfd);
        if (d->lastgroup != NULL) d->lastgroup->next = grouptmp; else sf->groups = grouptmp;
        d->lastgroup = grouptmp;
    }
    else if ( strmatch(tok,"GroupKern:")==0 ) {
        int temp = 0;
        struct ff_rawoffsets *kerntmp = calloc(1, sizeof(struct ff_rawoffsets));
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        kerntmp->left = SFDReadUTF7Str(sfd);
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        kerntmp->right = SFDReadUTF7Str(sfd);
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        getint(sfd,&temp);
        kerntmp->offset = temp;
        while ((ch=nlgetc(sfd)) == ' ' || ch == '\n'); ungetc(ch, sfd);
        if (d->lastgroupkern != NULL) d->lastgroupkern->next = kerntmp; else sf->groupkerns = kerntmp;
        d->lastgroupkern = kerntmp;
    }
    else if ( strmatch(tok,"GroupVKern:")==0 ) {
        int temp = 0;
        struct ff_rawoffsets *kerntmp = calloc(1, sizeof(struct ff_rawoffsets));
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        kerntmp->left = SFDReadUTF7Str(sfd);
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        kerntmp->right = SFDReadUTF7Str(sfd);
        while ((ch=nlgetc(sfd)) == ' '); ungetc(ch, sfd);
        getint(sfd,&temp);
        kerntmp->offset = temp;
        while ((ch=nlgetc(sfd)) == ' ' || ch == '\n'); ungetc(ch, sfd);
        if (d->lastgroupvkern != NULL) d->lastgroupvkern->next = kerntmp; else sf->groupvkerns = kerntmp;
        d->lastgroupvkern = kerntmp;
    }
    else if ( strmatch(tok,"MacIndic2:")==0 || strmatch(tok,"MacContext2:")==0 ||
	      strmatch(tok,"MacLigature2:")==0 || strmatch(tok,"MacSimple2:")==0 ||
	      strmatch(tok,"MacKern2:")==0 || strmatch(tok,"MacInsert2:")==0 ||
	      strmatch(tok,"MacIndic:")==0 || strmatch(tok,"MacContext:")==0 ||
	      strmatch(tok,"MacLigature:")==0 || strmatch(tok,"MacSimple:")==0 ||
	      strmatch(tok,"MacKern:")==0 || strmatch(tok,"MacInsert:")==0 )
    {
	ASM *sm;
	if ( strchr(tok,'2')!=NULL ) {
	    old = false;
	    sm = chunkalloc(sizeof(ASM));
	} else {
	    old = true;
	    sm = chunkalloc(sizeof(ASM1));
	}
	if ( (sf->sfd_version<2)!=old ) {
	    IError( "Version mixup in state machine of sfd file." );
	    exit(1);
	}
	if ( d->lastsm==NULL )
	    sf->sm = sm;
	else
	    d->lastsm->next = sm;
	d->lastsm = sm;
	SFDParseStateMachine(sfd,sf,sm,tok,old);
    }
    else if ( strmatch(tok,"MacFeat:")==0 )
    {
	sf->features = SFDParseMacFeatures(sfd,tok);
    }
    else if ( strmatch(tok,"TtfTable:")==0 )
    {
	/* Old, binary format */
	/* still used for maxp and unknown tables */
	SFDGetTtfTable(sfd,sf,d->lastttf);
    }
    else if ( strmatch(tok,"TtTable:")==0 )
    {
	/* text instruction format */
	SFDGetTtTable(sfd,sf,d->lastttf);
    }


    ///////////////////

    else if ( strmatch(tok,"ShortTable:")==0 )
    {
	// only read, not written.
	/* text number format */
	SFDGetShortTable(sfd,sf,d->lastttf);
    }
    else
    {
        //
        // We didn't have a match ourselves.
        //
        return false;
    }
    return true;
}