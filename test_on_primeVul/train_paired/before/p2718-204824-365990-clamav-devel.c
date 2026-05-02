static int parseicon(icon_groupset *set, uint32_t rva, cli_ctx *ctx, struct cli_exe_section *exe_sections, uint16_t nsections, uint32_t hdr_size) {
    struct {
	unsigned int sz;
	unsigned int w;
	unsigned int h;
	unsigned short planes; 
	unsigned short depth; 
	unsigned int comp;
	unsigned int imagesize;
	unsigned int dpix;
	unsigned int dpiy;
	unsigned int used;
	unsigned int important;
    } bmphdr;
    struct icomtr metrics;
    unsigned char *rawimage;
    const char *tempd;
    uint32_t *palette = NULL, *imagedata;
    unsigned int scanlinesz, andlinesz;
    unsigned int width, height, depth, x, y;
    unsigned int err, scalemode = 2, enginesize;
    fmap_t *map;
    uint32_t icoff;
    struct icon_matcher *matcher;
    unsigned int special_32_is_32 = 0;

    if(!ctx || !ctx->engine || !(matcher=ctx->engine->iconcheck))
	return CL_SUCCESS;
    map = *ctx->fmap;
    tempd = (cli_debug_flag && ctx->engine->keeptmp) ? (ctx->engine->tmpdir ? ctx->engine->tmpdir : cli_gettmpdir()) : NULL;
    icoff = cli_rawaddr(rva, exe_sections, nsections, &err, map->len, hdr_size);

    /* read the bitmap header */
    if(err || !(imagedata = fmap_need_off_once(map, icoff, 4))) {
	cli_dbgmsg("parseicon: offset to icon is out of file\n");
	return CL_SUCCESS;
    }

    rva = cli_readint32(imagedata);
    icoff = cli_rawaddr(rva, exe_sections, nsections, &err, map->len, hdr_size);
    if(err || fmap_readn(map, &bmphdr, icoff, sizeof(bmphdr)) != sizeof(bmphdr)) {
	cli_dbgmsg("parseicon: bmp header is out of file\n");
	return CL_SUCCESS;
    }

    if(READ32(bmphdr.sz) < sizeof(bmphdr)) {
	cli_dbgmsg("parseicon: BMP header too small\n");
	return CL_SUCCESS;
    }

    /* seek to the end of v4/v5 header */
    icoff += READ32(bmphdr.sz);

    width = READ32(bmphdr.w);
    height = READ32(bmphdr.h) / 2;
    depth = READ16(bmphdr.depth);
    if(width > 256 || height > 256 || width < 16 || height < 16) {
	cli_dbgmsg("parseicon: Image too small or too big (%ux%u)\n", width, height);
	return CL_SUCCESS;
    }
    if(width < height * 3 / 4 || height < width * 3 / 4) {
	cli_dbgmsg("parseicon: Image not square enough (%ux%u)\n", width, height);
	return CL_SUCCESS;
    }	

    /* scaling logic */
    if(width == height) {
	if(width == 16 || width == 24 || width == 32)
	    scalemode = 0;
	else if(!(width % 32) || !(width % 24))
	    scalemode = 1;
	else
	    scalemode = 2;
    }

    cli_dbgmsg("parseicon: Bitmap - %ux%ux%u\n", width, height, depth);

    /* check color depth and load palette */
    switch(depth) {
    default:
    case 0:
	/* PNG OR JPEG */
	cli_dbgmsg("parseicon: PNG icons are not yet sported\n");
	return CL_SUCCESS;
    case 1:
    case 4:
    case 8:
	/* HAVE PALETTE */
	if(!(palette = fmap_need_off(map, icoff, (1<<depth) * sizeof(int))))
	    return CL_SUCCESS;
	icoff += (1<<depth) * sizeof(int);
	/* for(j=0; j<pcolcnt; j++) */
	/* cli_dbgmsg("Palette[%u] = %08x\n", j, palette[j]); */
	break;
    case 16:
    case 24:
    case 32:
	/* NO PALETTE */
	break;
    }

    /* compute line sizes */
    scanlinesz = 4*(width * depth / 32) + 4*(width * depth % 32 != 0);
    andlinesz = ((depth & 0x1f) != 0) * (4*(width / 32) + 4*(width % 32 != 0));

    /* read the raw image */
    
    if(!(rawimage = fmap_need_off_once(map, icoff, height * (scanlinesz + andlinesz)))) {
	if(palette) fmap_unneed_ptr(map, palette, (1<<depth) * sizeof(int));
	return CL_SUCCESS;
    }
    if(!(imagedata = cli_malloc(width * height * sizeof(*imagedata)))) {
	if(palette) fmap_unneed_ptr(map, palette, (1<<depth) * sizeof(int));
	return CL_SUCCESS;
    }

    /* decode the image to an RGBA array */
    for(y=0; y<height; y++) {
	unsigned int x_off = y * scanlinesz;
	switch(depth) {
	case 1:
	case 4:
	case 8: {
	    unsigned int have = 0;
	    unsigned char c;
	    for(x=0; x<width; x++) {
		if(!have) {
		    c = rawimage[x_off++];
		    have = 8;
		}
		have -= depth;
		imagedata[(height - 1 - y) * width + x] = READ32(palette[(c >> have) & ((1<<depth)-1)]);
	    }
	    break;
	}
	case 16: {
	    for(x=0; x<width; x++) {
		unsigned int b = (rawimage[x_off] & 0x1f);
		unsigned int g = ((rawimage[x_off] >> 5) | ((rawimage[x_off+1] & 0x3)<<3));
		unsigned int r = (rawimage[x_off+1] & 0xfc);
		b = (b<<3) | (b>>2);
		g = ((g<<3) | (g>>2))<<11;
		r = ((r<<3) | (r>>2))<<17;
		imagedata[(height - 1 - y) * width + x] = r | g | b;
		x_off+=2;
	    }
	    break;
	}
	case 24:
	    for(x=0; x<width; x++) {
		unsigned int c = rawimage[x_off] | (rawimage[x_off+1]<<8) | (rawimage[x_off+2]<<16);
		imagedata[(height - 1 - y) * width + x] = c;
		x_off+=3;
	    }
	    break;
	case 32:
	    for(x=0; x<width; x++) {
		unsigned int a = rawimage[x_off + 3] << 24;
		imagedata[(height - 1 - y) * width + x] = rawimage[x_off] | (rawimage[x_off + 1] << 8) | (rawimage[x_off + 2] << 16) | a;
		special_32_is_32 |= a;
		x_off+=4;
	    }
	    break;
	}
    }

    if(palette) fmap_unneed_ptr(map, palette, (1<<depth) * sizeof(int));
    makebmp("0-noalpha", tempd, width, height, imagedata);

    if(depth == 32 && !special_32_is_32) { /* Sometimes it really is 24. Exploited live - see sample 0013839101 */
	andlinesz = 4*(width / 32) + 4*(width % 32 != 0);
	if(!(rawimage = fmap_need_off_once(map, icoff + height * scanlinesz, height * andlinesz))) {
	    /* Likely a broken sample - 32bit icon with 24bit data and a broken mask:
	       i could really break out here but i've got the full image, so i'm just forcing full alpha
	       Found in samples: 0008777448, 0009116157, 0009116157 */
	    for(y=0; y<height; y++)
		for(x=0; x<width; x++)
		    imagedata[y * width + x] |= 0xff000000;
	    special_32_is_32 = 1;
	    cli_dbgmsg("parseicon: found a broken and stupid icon\n");
	} else cli_dbgmsg("parseicon: found a stupid icon\n");
    } else rawimage += height * scanlinesz;

    /* Set alpha on or off based on the mask */
    if((depth & 0x1f) || !special_32_is_32) {
	for(y=0; y<height; y++) {
	    unsigned int x_off = y * andlinesz;
	    unsigned int have = 0;
	    unsigned char c;
	    for(x=0; x<width; x++) {
		if(!have) {
		    c = rawimage[x_off++];
		    have = 8;
		}
		have--;
		imagedata[(height - 1 - y) * width + x] |= (!((c >> have) & 1)) * 0xff000000;
	    }
	}
    }
    makebmp("1-alpha-mask", tempd, width, height, imagedata);

    /* Blend alpha */
    for(y=0; y<height; y++) {
	for(x=0; x<width; x++) {
	    unsigned int r, g, b, a;
	    unsigned int c = imagedata[y * width + x];
	    a = c >> 24;
	    r = (c >> 16) & 0xff;
	    g = (c >> 8) & 0xff;
	    b = c & 0xff;
	    r = 0xff - a + a * r / 0xff;
	    g = 0xff - a + a * g / 0xff;
	    b = 0xff - a + a * b / 0xff;
	    imagedata[y * width + x] = 0xff000000 | (r << 16) | (g << 8) | b;
	}
    }


    switch (scalemode) {
    case 0:
	break;
    case 1:
	/* Fast 50% scaler with linear gamma */
	while(width > 32) {
	    for(y=0; y<height; y+=2) {
		for(x=0; x<width; x+=2) {
		    unsigned int c1 = imagedata[y * width + x], c2 =imagedata[y * width + x + 1], c3 = imagedata[(y+1) * width + x], c4 = imagedata[(y+1) * width + x + 1];
		    c1 = (((c1 ^ c2) & 0xfefefefe)>>1) + (c1 & c2);
		    c2 = (((c3 ^ c4) & 0xfefefefe)>>1) + (c3 & c4);
		    imagedata[y/2 * width/2 + x/2] = (((c1 ^ c2) & 0xfefefefe)>>1) + (c1 & c2);
		}
	    }
	    width /= 2;
	    height /= 2;
	    cli_dbgmsg("parseicon: Fast scaling to %ux%u\n", width, height);
	}
	break;
    case 2:
	/* Slow up/down scale */
	{
	    double scalex, scaley;
	    unsigned int newsize;
	    uint32_t *newdata;

	    if(abs((int)width - 32) + abs((int)height - 32) < abs((int)width - 24) + abs((int)height - 24))
		newsize = 32;
	    else if(abs((int)width - 24) + abs((int)height - 24) < abs((int)width - 16) + abs((int)height - 16))
		newsize = 24;
	    else
		newsize = 16;
	    scalex = (double)width / newsize;
	    scaley = (double)height / newsize;
	    if(!(newdata = cli_malloc(newsize * newsize * sizeof(*newdata)))) {
		return CL_SUCCESS;
	    }
	    cli_dbgmsg("parseicon: Slow scaling to %ux%u (%f, %f)\n", newsize, newsize, scalex, scaley);
	    for(y = 0; y<newsize; y++) {
		unsigned int oldy = (unsigned int)(y * scaley) * width;
		for(x = 0; x<newsize; x++)
		    newdata[y*newsize + x] = imagedata[oldy + (unsigned int)(x * scalex + 0.5f)];
	    }
	    free(imagedata);
	    height = newsize;
	    width = newsize;
	    imagedata = newdata;
	}
    }
    makebmp("2-alpha-blend", tempd, width, height, imagedata);

    getmetrics(width, imagedata, &metrics, tempd);
    free(imagedata);

    enginesize = (width >> 3) - 2;
    for(x=0; x<matcher->icon_counts[enginesize]; x++) {
	unsigned int color = 0, gray = 0, bright, dark, edge, noedge, reds, greens, blues, ccount;
	unsigned int colors, confidence, bwmatch = 0, positivematch = 64 + 4*(2-enginesize);
	unsigned int i, j;

	i = matcher->icons[enginesize][x].group[0];
	j = i % 64;
	i /= 64;
	if(!(set->v[0][i] & ((uint64_t)1<<j))) continue;
	i = matcher->icons[enginesize][x].group[1];
	j = i % 64;
	i /= 64;
	if(!(set->v[1][i] & ((uint64_t)1<<j))) continue;
	
	if(!metrics.ccount && !matcher->icons[enginesize][x].ccount) {
	    /* BW matching */
	    edge = matchbwpoint(width, metrics.edge_x, metrics.edge_y, metrics.edge_avg, metrics.color_x, metrics.color_y, metrics.color_avg, matcher->icons[enginesize][x].edge_x, matcher->icons[enginesize][x].edge_y, matcher->icons[enginesize][x].edge_avg, matcher->icons[enginesize][x].color_x, matcher->icons[enginesize][x].color_y, matcher->icons[enginesize][x].color_avg);
	    noedge = matchbwpoint(width, metrics.noedge_x, metrics.noedge_y, metrics.noedge_avg, metrics.gray_x, metrics.gray_y, metrics.gray_avg, matcher->icons[enginesize][x].noedge_x, matcher->icons[enginesize][x].noedge_y, matcher->icons[enginesize][x].noedge_avg, matcher->icons[enginesize][x].gray_x, matcher->icons[enginesize][x].gray_y, matcher->icons[enginesize][x].gray_avg);
	    bwmatch = 1;
	} else {
	    edge = matchpoint(width, metrics.edge_x, metrics.edge_y, metrics.edge_avg, matcher->icons[enginesize][x].edge_x, matcher->icons[enginesize][x].edge_y, matcher->icons[enginesize][x].edge_avg, 255);
	    noedge = matchpoint(width, metrics.noedge_x, metrics.noedge_y, metrics.noedge_avg, matcher->icons[enginesize][x].noedge_x, matcher->icons[enginesize][x].noedge_y, matcher->icons[enginesize][x].noedge_avg, 255);
	    if(metrics.ccount && matcher->icons[enginesize][x].ccount) {
		/* color matching */
		color = matchpoint(width, metrics.color_x, metrics.color_y, metrics.color_avg, matcher->icons[enginesize][x].color_x, matcher->icons[enginesize][x].color_y, matcher->icons[enginesize][x].color_avg, 4072);
		gray = matchpoint(width, metrics.gray_x, metrics.gray_y, metrics.gray_avg, matcher->icons[enginesize][x].gray_x, matcher->icons[enginesize][x].gray_y, matcher->icons[enginesize][x].gray_avg, 4072);
	    }
	}

	bright = matchpoint(width, metrics.bright_x, metrics.bright_y, metrics.bright_avg, matcher->icons[enginesize][x].bright_x, matcher->icons[enginesize][x].bright_y, matcher->icons[enginesize][x].bright_avg, 255);
	dark = matchpoint(width, metrics.dark_x, metrics.dark_y, metrics.dark_avg, matcher->icons[enginesize][x].dark_x, matcher->icons[enginesize][x].dark_y, matcher->icons[enginesize][x].dark_avg, 255);

	reds = abs((int)metrics.rsum - (int)matcher->icons[enginesize][x].rsum) * 10;
	reds = (reds < 100) * (100 - reds);
	greens = abs((int)metrics.gsum - (int)matcher->icons[enginesize][x].gsum) * 10;
	greens = (greens < 100) * (100 - greens);
	blues = abs((int)metrics.bsum - (int)matcher->icons[enginesize][x].bsum) * 10;
	blues = (blues < 100) * (100 - blues);
	ccount = abs((int)metrics.ccount - (int)matcher->icons[enginesize][x].ccount) * 10;
	ccount = (ccount < 100) * (100 - ccount);
	colors = (reds + greens + blues + ccount) / 4;

	if(bwmatch) {
	    confidence = (bright + dark + edge * 2 + noedge) / 6;
	    positivematch = 70;
	} else
	    confidence = (color + (gray + bright + noedge)*2/3 + dark + edge + colors) / 6;

	cli_dbgmsg("parseicon: edge confidence: %u%%\n", edge);
	cli_dbgmsg("parseicon: noedge confidence: %u%%\n", noedge);
	if(!bwmatch) {
	    cli_dbgmsg("parseicon: color confidence: %u%%\n", color);
	    cli_dbgmsg("parseicon: gray confidence: %u%%\n", gray);
	}
	cli_dbgmsg("parseicon: bright confidence: %u%%\n", bright);
	cli_dbgmsg("parseicon: dark confidence: %u%%\n", dark);
	if(!bwmatch)
	    cli_dbgmsg("parseicon: spread confidence: red %u%%, green %u%%, blue %u%% - colors %u%%\n", reds, greens, blues, ccount);

	if(confidence >= positivematch) {
	    cli_dbgmsg("confidence: %u\n", confidence);

	    cli_append_virus(ctx,matcher->icons[enginesize][x].name);
	    return CL_VIRUS;
	}
    }

    return CL_SUCCESS;
}