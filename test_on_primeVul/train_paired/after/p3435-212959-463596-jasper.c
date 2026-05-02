int jp2_encode(jas_image_t *image, jas_stream_t *out, const char *optstr)
{
	jp2_box_t *box;
	jp2_ftyp_t *ftyp;
	jp2_ihdr_t *ihdr;
	jas_stream_t *tmpstream;
	int allcmptssame;
	jp2_bpcc_t *bpcc;
	long len;
	uint_fast16_t cmptno;
	jp2_colr_t *colr;
	char buf[4096];
	uint_fast32_t overhead;
	jp2_cdefchan_t *cdefchanent;
	jp2_cdef_t *cdef;
	int i;
	uint_fast32_t typeasoc;
	jas_iccprof_t *iccprof;
	jas_stream_t *iccstream;
	int pos;
	int needcdef;
	int prec;
	int sgnd;

	box = 0;
	tmpstream = 0;
	iccstream = 0;
	iccprof = 0;

	allcmptssame = 1;
	sgnd = jas_image_cmptsgnd(image, 0);
	prec = jas_image_cmptprec(image, 0);
	for (i = 1; i < jas_image_numcmpts(image); ++i) {
		if (jas_image_cmptsgnd(image, i) != sgnd ||
		  jas_image_cmptprec(image, i) != prec) {
			allcmptssame = 0;
			break;
		}
	}

	/* Output the signature box. */

	if (!(box = jp2_box_create(JP2_BOX_JP))) {
		goto error;
	}
	box->data.jp.magic = JP2_JP_MAGIC;
	if (jp2_box_put(box, out)) {
		goto error;
	}
	jp2_box_destroy(box);
	box = 0;

	/* Output the file type box. */

	if (!(box = jp2_box_create(JP2_BOX_FTYP))) {
		goto error;
	}
	ftyp = &box->data.ftyp;
	ftyp->majver = JP2_FTYP_MAJVER;
	ftyp->minver = JP2_FTYP_MINVER;
	ftyp->numcompatcodes = 1;
	ftyp->compatcodes[0] = JP2_FTYP_COMPATCODE;
	if (jp2_box_put(box, out)) {
		goto error;
	}
	jp2_box_destroy(box);
	box = 0;

	/*
	 * Generate the data portion of the JP2 header box.
	 * We cannot simply output the header for this box
	 * since we do not yet know the correct value for the length
	 * field.
	 */

	if (!(tmpstream = jas_stream_memopen(0, 0))) {
		goto error;
	}

	/* Generate image header box. */

	if (!(box = jp2_box_create(JP2_BOX_IHDR))) {
		goto error;
	}
	ihdr = &box->data.ihdr;
	ihdr->width = jas_image_width(image);
	ihdr->height = jas_image_height(image);
	ihdr->numcmpts = jas_image_numcmpts(image);
	ihdr->bpc = allcmptssame ? JP2_SPTOBPC(jas_image_cmptsgnd(image, 0),
	  jas_image_cmptprec(image, 0)) : JP2_IHDR_BPCNULL;
	ihdr->comptype = JP2_IHDR_COMPTYPE;
	ihdr->csunk = 0;
	ihdr->ipr = 0;
	if (jp2_box_put(box, tmpstream)) {
		goto error;
	}
	jp2_box_destroy(box);
	box = 0;

	/* Generate bits per component box. */

	if (!allcmptssame) {
		if (!(box = jp2_box_create(JP2_BOX_BPCC))) {
			goto error;
		}
		bpcc = &box->data.bpcc;
		bpcc->numcmpts = jas_image_numcmpts(image);
		if (!(bpcc->bpcs = jas_alloc2(bpcc->numcmpts,
		  sizeof(uint_fast8_t)))) {
			goto error;
		}
		for (cmptno = 0; cmptno < bpcc->numcmpts; ++cmptno) {
			bpcc->bpcs[cmptno] = JP2_SPTOBPC(jas_image_cmptsgnd(image,
			  cmptno), jas_image_cmptprec(image, cmptno));
		}
		if (jp2_box_put(box, tmpstream)) {
			goto error;
		}
		jp2_box_destroy(box);
		box = 0;
	}

	/* Generate color specification box. */

	if (!(box = jp2_box_create(JP2_BOX_COLR))) {
		goto error;
	}
	colr = &box->data.colr;
	switch (jas_image_clrspc(image)) {
	case JAS_CLRSPC_SRGB:
	case JAS_CLRSPC_SYCBCR:
	case JAS_CLRSPC_SGRAY:
		colr->method = JP2_COLR_ENUM;
		colr->csid = clrspctojp2(jas_image_clrspc(image));
		colr->pri = JP2_COLR_PRI;
		colr->approx = 0;
		break;
	default:
		colr->method = JP2_COLR_ICC;
		colr->pri = JP2_COLR_PRI;
		colr->approx = 0;
		/* Ensure that cmprof_ is not null. */
		if (!jas_image_cmprof(image)) {
			goto error;
		}
		if (!(iccprof = jas_iccprof_createfromcmprof(
		  jas_image_cmprof(image)))) {
			goto error;
		}
		if (!(iccstream = jas_stream_memopen(0, 0))) {
			goto error;
		}
		if (jas_iccprof_save(iccprof, iccstream)) {
			goto error;
		}
		if ((pos = jas_stream_tell(iccstream)) < 0) {
			goto error;
		}
		colr->iccplen = pos;
		if (!(colr->iccp = jas_malloc(pos))) {
			goto error;
		}
		jas_stream_rewind(iccstream);
		if (jas_stream_read(iccstream, colr->iccp, colr->iccplen) !=
		  colr->iccplen) {
			goto error;
		}
		jas_stream_close(iccstream);
		iccstream = 0;
		jas_iccprof_destroy(iccprof);
		iccprof = 0;
		break;
	}
	if (jp2_box_put(box, tmpstream)) {
		goto error;
	}
	jp2_box_destroy(box);
	box = 0;

	needcdef = 1;
	switch (jas_clrspc_fam(jas_image_clrspc(image))) {
	case JAS_CLRSPC_FAM_RGB:
		if (jas_image_cmpttype(image, 0) ==
		  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_R) &&
		  jas_image_cmpttype(image, 1) ==
		  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_G) &&
		  jas_image_cmpttype(image, 2) ==
		  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_B))
			needcdef = 0;
		break;
	case JAS_CLRSPC_FAM_YCBCR:
		if (jas_image_cmpttype(image, 0) ==
		  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_YCBCR_Y) &&
		  jas_image_cmpttype(image, 1) ==
		  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_YCBCR_CB) &&
		  jas_image_cmpttype(image, 2) ==
		  JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_YCBCR_CR))
			needcdef = 0;
		break;
	case JAS_CLRSPC_FAM_GRAY:
		if (jas_image_cmpttype(image, 0) ==
		  JAS_IMAGE_CT_COLOR(JAS_IMAGE_CT_GRAY_Y))
			needcdef = 0;
		break;
	default:
		abort();
		break;
	}

	if (needcdef) {
		if (!(box = jp2_box_create(JP2_BOX_CDEF))) {
			goto error;
		}
		cdef = &box->data.cdef;
		cdef->numchans = jas_image_numcmpts(image);
		cdef->ents = jas_alloc2(cdef->numchans, sizeof(jp2_cdefchan_t));
		for (i = 0; i < jas_image_numcmpts(image); ++i) {
			cdefchanent = &cdef->ents[i];
			cdefchanent->channo = i;
			typeasoc = jp2_gettypeasoc(jas_image_clrspc(image), jas_image_cmpttype(image, i));
			cdefchanent->type = typeasoc >> 16;
			cdefchanent->assoc = typeasoc & 0x7fff;
		}
		if (jp2_box_put(box, tmpstream)) {
			goto error;
		}
		jp2_box_destroy(box);
		box = 0;
	}

	/* Determine the total length of the JP2 header box. */

	len = jas_stream_tell(tmpstream);
	jas_stream_rewind(tmpstream);

	/*
	 * Output the JP2 header box and all of the boxes which it contains.
	 */

	if (!(box = jp2_box_create(JP2_BOX_JP2H))) {
		goto error;
	}
	box->len = len + JP2_BOX_HDRLEN(false);
	if (jp2_box_put(box, out)) {
		goto error;
	}
	jp2_box_destroy(box);
	box = 0;

	if (jas_stream_copy(out, tmpstream, len)) {
		goto error;
	}

	jas_stream_close(tmpstream);
	tmpstream = 0;

	/*
	 * Output the contiguous code stream box.
	 */

	if (!(box = jp2_box_create(JP2_BOX_JP2C))) {
		goto error;
	}
	box->len = 0;
	if (jp2_box_put(box, out)) {
		goto error;
	}
	jp2_box_destroy(box);
	box = 0;

	/* Output the JPEG-2000 code stream. */

	overhead = jas_stream_getrwcount(out);
	sprintf(buf, "%s\n_jp2overhead=%lu\n", (optstr ? optstr : ""),
	  (unsigned long) overhead);

	if (jpc_encode(image, out, buf)) {
		goto error;
	}

	return 0;

error:

	if (iccprof) {
		jas_iccprof_destroy(iccprof);
	}
	if (iccstream) {
		jas_stream_close(iccstream);
	}
	if (box) {
		jp2_box_destroy(box);
	}
	if (tmpstream) {
		jas_stream_close(tmpstream);
	}
	return -1;
}