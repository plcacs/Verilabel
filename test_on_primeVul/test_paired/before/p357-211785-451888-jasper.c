static jpc_enc_cp_t *cp_create(const char *optstr, jas_image_t *image)
{
	jpc_enc_cp_t *cp;
	jas_tvparser_t *tvp;
	int ret;
	int numilyrrates;
	double *ilyrrates;
	int i;
	int tagid;
	jpc_enc_tcp_t *tcp;
	jpc_enc_tccp_t *tccp;
	jpc_enc_ccp_t *ccp;
	uint_fast16_t rlvlno;
	uint_fast16_t prcwidthexpn;
	uint_fast16_t prcheightexpn;
	bool enablemct;
	uint_fast32_t jp2overhead;
	uint_fast16_t lyrno;
	uint_fast32_t hsteplcm;
	uint_fast32_t vsteplcm;
	bool mctvalid;

	tvp = 0;
	cp = 0;
	ilyrrates = 0;
	numilyrrates = 0;

	if (!(cp = jas_malloc(sizeof(jpc_enc_cp_t)))) {
		goto error;
	}

	prcwidthexpn = 15;
	prcheightexpn = 15;
	enablemct = true;
	jp2overhead = 0;

	cp->ccps = 0;
	cp->debug = 0;
	cp->imgareatlx = UINT_FAST32_MAX;
	cp->imgareatly = UINT_FAST32_MAX;
	cp->refgrdwidth = 0;
	cp->refgrdheight = 0;
	cp->tilegrdoffx = UINT_FAST32_MAX;
	cp->tilegrdoffy = UINT_FAST32_MAX;
	cp->tilewidth = 0;
	cp->tileheight = 0;
	cp->numcmpts = jas_image_numcmpts(image);

	hsteplcm = 1;
	vsteplcm = 1;
	for (unsigned cmptno = 0; cmptno < jas_image_numcmpts(image); ++cmptno) {
		if (jas_image_cmptbrx(image, cmptno) + jas_image_cmpthstep(image, cmptno) <=
		  jas_image_brx(image) || jas_image_cmptbry(image, cmptno) +
		  jas_image_cmptvstep(image, cmptno) <= jas_image_bry(image)) {
			jas_eprintf("unsupported image type\n");
			goto error;
		}
		/* Note: We ought to be calculating the LCMs here.  Fix some day. */
		hsteplcm *= jas_image_cmpthstep(image, cmptno);
		vsteplcm *= jas_image_cmptvstep(image, cmptno);
	}

	if (!(cp->ccps = jas_alloc2(cp->numcmpts, sizeof(jpc_enc_ccp_t)))) {
		goto error;
	}
	unsigned cmptno;
	for (cmptno = 0, ccp = cp->ccps; cmptno < cp->numcmpts; ++cmptno,
	  ++ccp) {
		ccp->sampgrdstepx = jas_image_cmpthstep(image, cmptno);
		ccp->sampgrdstepy = jas_image_cmptvstep(image, cmptno);
		/* XXX - this isn't quite correct for more general image */
		ccp->sampgrdsubstepx = 0;
		ccp->sampgrdsubstepx = 0;
		ccp->prec = jas_image_cmptprec(image, cmptno);
		ccp->sgnd = jas_image_cmptsgnd(image, cmptno);
		ccp->numstepsizes = 0;
		memset(ccp->stepsizes, 0, sizeof(ccp->stepsizes));
	}

	cp->rawsize = jas_image_rawsize(image);
	if (cp->rawsize == 0) {
		/* prevent division by zero in cp_create() */
		goto error;
	}
	cp->totalsize = UINT_FAST32_MAX;

	tcp = &cp->tcp;
	tcp->csty = 0;
	tcp->intmode = true;
	tcp->prg = JPC_COD_LRCPPRG;
	tcp->numlyrs = 1;
	tcp->ilyrrates = 0;

	tccp = &cp->tccp;
	tccp->csty = 0;
	tccp->maxrlvls = 6;
	tccp->cblkwidthexpn = 6;
	tccp->cblkheightexpn = 6;
	tccp->cblksty = 0;
	tccp->numgbits = 2;

	if (!(tvp = jas_tvparser_create(optstr ? optstr : ""))) {
		goto error;
	}

	while (!(ret = jas_tvparser_next(tvp))) {
		switch (jas_taginfo_nonull(jas_taginfos_lookup(encopts,
		  jas_tvparser_gettag(tvp)))->id) {
		case OPT_DEBUG:
			cp->debug = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_IMGAREAOFFX:
			cp->imgareatlx = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_IMGAREAOFFY:
			cp->imgareatly = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_TILEGRDOFFX:
			cp->tilegrdoffx = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_TILEGRDOFFY:
			cp->tilegrdoffy = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_TILEWIDTH:
			cp->tilewidth = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_TILEHEIGHT:
			cp->tileheight = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_PRCWIDTH:
			prcwidthexpn = jpc_floorlog2(atoi(jas_tvparser_getval(tvp)));
			break;
		case OPT_PRCHEIGHT:
			prcheightexpn = jpc_floorlog2(atoi(jas_tvparser_getval(tvp)));
			break;
		case OPT_CBLKWIDTH:
			tccp->cblkwidthexpn =
			  jpc_floorlog2(atoi(jas_tvparser_getval(tvp)));
			break;
		case OPT_CBLKHEIGHT:
			tccp->cblkheightexpn =
			  jpc_floorlog2(atoi(jas_tvparser_getval(tvp)));
			break;
		case OPT_MODE:
			if ((tagid = jas_taginfo_nonull(jas_taginfos_lookup(modetab,
			  jas_tvparser_getval(tvp)))->id) < 0) {
				jas_eprintf("ignoring invalid mode %s\n",
				  jas_tvparser_getval(tvp));
			} else {
				tcp->intmode = (tagid == MODE_INT);
			}
			break;
		case OPT_PRG:
			if ((tagid = jas_taginfo_nonull(jas_taginfos_lookup(prgordtab,
			  jas_tvparser_getval(tvp)))->id) < 0) {
				jas_eprintf("ignoring invalid progression order %s\n",
				  jas_tvparser_getval(tvp));
			} else {
				tcp->prg = tagid;
			}
			break;
		case OPT_NOMCT:
			enablemct = false;
			break;
		case OPT_MAXRLVLS:
			tccp->maxrlvls = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_SOP:
			cp->tcp.csty |= JPC_COD_SOP;
			break;
		case OPT_EPH:
			cp->tcp.csty |= JPC_COD_EPH;
			break;
		case OPT_LAZY:
			tccp->cblksty |= JPC_COX_LAZY;
			break;
		case OPT_TERMALL:
			tccp->cblksty |= JPC_COX_TERMALL;
			break;
		case OPT_SEGSYM:
			tccp->cblksty |= JPC_COX_SEGSYM;
			break;
		case OPT_VCAUSAL:
			tccp->cblksty |= JPC_COX_VSC;
			break;
		case OPT_RESET:
			tccp->cblksty |= JPC_COX_RESET;
			break;
		case OPT_PTERM:
			tccp->cblksty |= JPC_COX_PTERM;
			break;
		case OPT_NUMGBITS:
			cp->tccp.numgbits = atoi(jas_tvparser_getval(tvp));
			break;
		case OPT_RATE:
			if (ratestrtosize(jas_tvparser_getval(tvp), cp->rawsize,
			  &cp->totalsize)) {
				jas_eprintf("ignoring bad rate specifier %s\n",
				  jas_tvparser_getval(tvp));
			}
			break;
		case OPT_ILYRRATES:
			if (jpc_atoaf(jas_tvparser_getval(tvp), &numilyrrates,
			  &ilyrrates)) {
				jas_eprintf("warning: invalid intermediate layer rates specifier ignored (%s)\n",
				  jas_tvparser_getval(tvp));
			}
			break;

		case OPT_JP2OVERHEAD:
			jp2overhead = atoi(jas_tvparser_getval(tvp));
			break;
		default:
			jas_eprintf("warning: ignoring invalid option %s\n",
			 jas_tvparser_gettag(tvp));
			break;
		}
	}

	jas_tvparser_destroy(tvp);
	tvp = 0;

	if (cp->totalsize != UINT_FAST32_MAX) {
		cp->totalsize = (cp->totalsize > jp2overhead) ?
		  (cp->totalsize - jp2overhead) : 0;
	}

	if (cp->imgareatlx == UINT_FAST32_MAX) {
		cp->imgareatlx = 0;
	} else {
		if (hsteplcm != 1) {
			jas_eprintf("warning: overriding imgareatlx value\n");
		}
		cp->imgareatlx *= hsteplcm;
	}
	if (cp->imgareatly == UINT_FAST32_MAX) {
		cp->imgareatly = 0;
	} else {
		if (vsteplcm != 1) {
			jas_eprintf("warning: overriding imgareatly value\n");
		}
		cp->imgareatly *= vsteplcm;
	}
	cp->refgrdwidth = cp->imgareatlx + jas_image_width(image);
	cp->refgrdheight = cp->imgareatly + jas_image_height(image);
	if (cp->tilegrdoffx == UINT_FAST32_MAX) {
		cp->tilegrdoffx = cp->imgareatlx;
	}
	if (cp->tilegrdoffy == UINT_FAST32_MAX) {
		cp->tilegrdoffy = cp->imgareatly;
	}
	if (!cp->tilewidth) {
		cp->tilewidth = cp->refgrdwidth - cp->tilegrdoffx;
	}
	if (!cp->tileheight) {
		cp->tileheight = cp->refgrdheight - cp->tilegrdoffy;
	}

	if (cp->numcmpts == 3) {
		mctvalid = true;
		for (cmptno = 0; cmptno < jas_image_numcmpts(image); ++cmptno) {
			if (jas_image_cmptprec(image, cmptno) != jas_image_cmptprec(image, 0) ||
			  jas_image_cmptsgnd(image, cmptno) != jas_image_cmptsgnd(image, 0) ||
			  jas_image_cmptwidth(image, cmptno) != jas_image_cmptwidth(image, 0) ||
			  jas_image_cmptheight(image, cmptno) != jas_image_cmptheight(image, 0)) {
				mctvalid = false;
			}
		}
	} else {
		mctvalid = false;
	}
	if (mctvalid && enablemct && jas_clrspc_fam(jas_image_clrspc(image)) != JAS_CLRSPC_FAM_RGB) {
		jas_eprintf("warning: color space apparently not RGB\n");
	}
	if (mctvalid && enablemct && jas_clrspc_fam(jas_image_clrspc(image)) == JAS_CLRSPC_FAM_RGB) {
		tcp->mctid = (tcp->intmode) ? (JPC_MCT_RCT) : (JPC_MCT_ICT);
	} else {
		tcp->mctid = JPC_MCT_NONE;
	}
	tccp->qmfbid = (tcp->intmode) ? (JPC_COX_RFT) : (JPC_COX_INS);

	for (rlvlno = 0; rlvlno < tccp->maxrlvls; ++rlvlno) {
		tccp->prcwidthexpns[rlvlno] = prcwidthexpn;
		tccp->prcheightexpns[rlvlno] = prcheightexpn;
	}
	if (prcwidthexpn != 15 || prcheightexpn != 15) {
		tccp->csty |= JPC_COX_PRT;
	}

	/* Ensure that the tile width and height is valid. */
	if (!cp->tilewidth) {
		jas_eprintf("invalid tile width %lu\n", (unsigned long)
		  cp->tilewidth);
		goto error;
	}
	if (!cp->tileheight) {
		jas_eprintf("invalid tile height %lu\n", (unsigned long)
		  cp->tileheight);
		goto error;
	}

	/* Ensure that the tile grid offset is valid. */
	if (cp->tilegrdoffx > cp->imgareatlx ||
	  cp->tilegrdoffy > cp->imgareatly ||
	  cp->tilegrdoffx + cp->tilewidth < cp->imgareatlx ||
	  cp->tilegrdoffy + cp->tileheight < cp->imgareatly) {
		jas_eprintf("invalid tile grid offset (%lu, %lu)\n",
		  (unsigned long) cp->tilegrdoffx, (unsigned long)
		  cp->tilegrdoffy);
		goto error;
	}

	cp->numhtiles = JPC_CEILDIV(cp->refgrdwidth - cp->tilegrdoffx,
	  cp->tilewidth);
	cp->numvtiles = JPC_CEILDIV(cp->refgrdheight - cp->tilegrdoffy,
	  cp->tileheight);
	cp->numtiles = cp->numhtiles * cp->numvtiles;

	if (ilyrrates && numilyrrates > 0) {
		tcp->numlyrs = numilyrrates + 1;
		if (!(tcp->ilyrrates = jas_alloc2((tcp->numlyrs - 1),
		  sizeof(jpc_fix_t)))) {
			goto error;
		}
		for (i = 0; i < JAS_CAST(int, tcp->numlyrs - 1); ++i) {
			tcp->ilyrrates[i] = jpc_dbltofix(ilyrrates[i]);
		}
	}

	/* Ensure that the integer mode is used in the case of lossless
	  coding. */
	if (cp->totalsize == UINT_FAST32_MAX && (!cp->tcp.intmode)) {
		jas_eprintf("cannot use real mode for lossless coding\n");
		goto error;
	}

	/* Ensure that the precinct width is valid. */
	if (prcwidthexpn > 15) {
		jas_eprintf("invalid precinct width\n");
		goto error;
	}

	/* Ensure that the precinct height is valid. */
	if (prcheightexpn > 15) {
		jas_eprintf("invalid precinct height\n");
		goto error;
	}

	/* Ensure that the code block width is valid. */
	if (cp->tccp.cblkwidthexpn < 2 || cp->tccp.cblkwidthexpn > 12) {
		jas_eprintf("invalid code block width %d\n",
		  JPC_POW2(cp->tccp.cblkwidthexpn));
		goto error;
	}

	/* Ensure that the code block height is valid. */
	if (cp->tccp.cblkheightexpn < 2 || cp->tccp.cblkheightexpn > 12) {
		jas_eprintf("invalid code block height %d\n",
		  JPC_POW2(cp->tccp.cblkheightexpn));
		goto error;
	}

	/* Ensure that the code block size is not too large. */
	if (cp->tccp.cblkwidthexpn + cp->tccp.cblkheightexpn > 12) {
		jas_eprintf("code block size too large\n");
		goto error;
	}

	/* Ensure that the number of layers is valid. */
	if (cp->tcp.numlyrs > 16384) {
		jas_eprintf("too many layers\n");
		goto error;
	}

	/* There must be at least one resolution level. */
	if (cp->tccp.maxrlvls < 1) {
		jas_eprintf("must be at least one resolution level\n");
		goto error;
	}

	/* Ensure that the number of guard bits is valid. */
	if (cp->tccp.numgbits > 8) {
		jas_eprintf("invalid number of guard bits\n");
		goto error;
	}

	/* Ensure that the rate is within the legal range. */
	if (cp->totalsize != UINT_FAST32_MAX && cp->totalsize > cp->rawsize) {
		jas_eprintf("warning: specified rate is unreasonably large (%lu > %lu)\n", (unsigned long) cp->totalsize, (unsigned long) cp->rawsize);
	}

	/* Ensure that the intermediate layer rates are valid. */
	if (tcp->numlyrs > 1) {
		/* The intermediate layers rates must increase monotonically. */
		for (lyrno = 0; lyrno + 2 < tcp->numlyrs; ++lyrno) {
			if (tcp->ilyrrates[lyrno] >= tcp->ilyrrates[lyrno + 1]) {
				jas_eprintf("intermediate layer rates must increase monotonically\n");
				goto error;
			}
		}
		/* The intermediate layer rates must be less than the overall rate. */
		if (cp->totalsize != UINT_FAST32_MAX) {
			for (lyrno = 0; lyrno < tcp->numlyrs - 1; ++lyrno) {
				if (jpc_fixtodbl(tcp->ilyrrates[lyrno]) > ((double) cp->totalsize)
				  / cp->rawsize) {
					jas_eprintf("warning: intermediate layer rates must be less than overall rate\n");
					goto error;
				}
			}
		}
	}

	if (ilyrrates) {
		jas_free(ilyrrates);
	}

	return cp;

error:

	if (ilyrrates) {
		jas_free(ilyrrates);
	}
	if (tvp) {
		jas_tvparser_destroy(tvp);
	}
	if (cp) {
		jpc_enc_cp_destroy(cp);
	}
	return 0;
}