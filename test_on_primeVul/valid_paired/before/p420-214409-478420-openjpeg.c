opj_image_t *pngtoimage(const char *read_idf, opj_cparameters_t * params)
{
    png_structp  png = NULL;
    png_infop    info = NULL;
    double gamma;
    int bit_depth, interlace_type, compression_type, filter_type;
    OPJ_UINT32 i;
    png_uint_32  width, height = 0U;
    int color_type;
    FILE *reader = NULL;
    OPJ_BYTE** rows = NULL;
    OPJ_INT32* row32s = NULL;
    /* j2k: */
    opj_image_t *image = NULL;
    opj_image_cmptparm_t cmptparm[4];
    OPJ_UINT32 nr_comp;
    OPJ_BYTE sigbuf[8];
    convert_XXx32s_C1R cvtXXTo32s = NULL;
    convert_32s_CXPX cvtCxToPx = NULL;
    OPJ_INT32* planes[4];

    if ((reader = fopen(read_idf, "rb")) == NULL) {
        fprintf(stderr, "pngtoimage: can not open %s\n", read_idf);
        return NULL;
    }

    if (fread(sigbuf, 1, MAGIC_SIZE, reader) != MAGIC_SIZE
            || memcmp(sigbuf, PNG_MAGIC, MAGIC_SIZE) != 0) {
        fprintf(stderr, "pngtoimage: %s is no valid PNG file\n", read_idf);
        goto fin;
    }

    if ((png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                      NULL, NULL, NULL)) == NULL) {
        goto fin;
    }
    if ((info = png_create_info_struct(png)) == NULL) {
        goto fin;
    }

    if (setjmp(png_jmpbuf(png))) {
        goto fin;
    }

    png_init_io(png, reader);
    png_set_sig_bytes(png, MAGIC_SIZE);

    png_read_info(png, info);

    if (png_get_IHDR(png, info, &width, &height,
                     &bit_depth, &color_type, &interlace_type,
                     &compression_type, &filter_type) == 0) {
        goto fin;
    }

    /* png_set_expand():
     * expand paletted images to RGB, expand grayscale images of
     * less than 8-bit depth to 8-bit depth, and expand tRNS chunks
     * to alpha channels.
     */
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_expand(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_expand(png);
    }
    /* We might wan't to expand background */
    /*
    if(png_get_valid(png, info, PNG_INFO_bKGD)) {
        png_color_16p bgnd;
        png_get_bKGD(png, info, &bgnd);
        png_set_background(png, bgnd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    }
    */

    if (!png_get_gAMA(png, info, &gamma)) {
        gamma = 1.0;
    }

    /* we're not displaying but converting, screen gamma == 1.0 */
    png_set_gamma(png, 1.0, gamma);

    png_read_update_info(png, info);

    color_type = png_get_color_type(png, info);

    switch (color_type) {
    case PNG_COLOR_TYPE_GRAY:
        nr_comp = 1;
        break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        nr_comp = 2;
        break;
    case PNG_COLOR_TYPE_RGB:
        nr_comp = 3;
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
        nr_comp = 4;
        break;
    default:
        fprintf(stderr, "pngtoimage: colortype %d is not supported\n", color_type);
        goto fin;
    }
    cvtCxToPx = convert_32s_CXPX_LUT[nr_comp];
    bit_depth = png_get_bit_depth(png, info);

    switch (bit_depth) {
    case 1:
    case 2:
    case 4:
    case 8:
        cvtXXTo32s = convert_XXu32s_C1R_LUT[bit_depth];
        break;
    case 16: /* 16 bpp is specific to PNG */
        cvtXXTo32s = convert_16u32s_C1R;
        break;
    default:
        fprintf(stderr, "pngtoimage: bit depth %d is not supported\n", bit_depth);
        goto fin;
    }


    rows = (OPJ_BYTE**)calloc(height + 1, sizeof(OPJ_BYTE*));
    if (rows == NULL) {
        fprintf(stderr, "pngtoimage: memory out\n");
        goto fin;
    }
    for (i = 0; i < height; ++i) {
        rows[i] = (OPJ_BYTE*)malloc(png_get_rowbytes(png, info));
        if (rows[i] == NULL) {
            fprintf(stderr, "pngtoimage: memory out\n");
            goto fin;
        }
    }
    png_read_image(png, rows);

    /* Create image */
    memset(cmptparm, 0, sizeof(cmptparm));
    for (i = 0; i < nr_comp; ++i) {
        cmptparm[i].prec = (OPJ_UINT32)bit_depth;
        /* bits_per_pixel: 8 or 16 */
        cmptparm[i].bpp = (OPJ_UINT32)bit_depth;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = (OPJ_UINT32)params->subsampling_dx;
        cmptparm[i].dy = (OPJ_UINT32)params->subsampling_dy;
        cmptparm[i].w = (OPJ_UINT32)width;
        cmptparm[i].h = (OPJ_UINT32)height;
    }

    image = opj_image_create(nr_comp, &cmptparm[0],
                             (nr_comp > 2U) ? OPJ_CLRSPC_SRGB : OPJ_CLRSPC_GRAY);
    if (image == NULL) {
        goto fin;
    }
    image->x0 = (OPJ_UINT32)params->image_offset_x0;
    image->y0 = (OPJ_UINT32)params->image_offset_y0;
    image->x1 = (OPJ_UINT32)(image->x0 + (width  - 1) * (OPJ_UINT32)
                             params->subsampling_dx + 1 + image->x0);
    image->y1 = (OPJ_UINT32)(image->y0 + (height - 1) * (OPJ_UINT32)
                             params->subsampling_dy + 1 + image->y0);

    row32s = (OPJ_INT32 *)malloc((size_t)width * nr_comp * sizeof(OPJ_INT32));
    if (row32s == NULL) {
        goto fin;
    }

    /* Set alpha channel */
    image->comps[nr_comp - 1U].alpha = 1U - (nr_comp & 1U);

    for (i = 0; i < nr_comp; i++) {
        planes[i] = image->comps[i].data;
    }

    for (i = 0; i < height; ++i) {
        cvtXXTo32s(rows[i], row32s, (OPJ_SIZE_T)width * nr_comp);
        cvtCxToPx(row32s, planes, width);
        planes[0] += width;
        planes[1] += width;
        planes[2] += width;
        planes[3] += width;
    }
fin:
    if (rows) {
        for (i = 0; i < height; ++i)
            if (rows[i]) {
                free(rows[i]);
            }
        free(rows);
    }
    if (row32s) {
        free(row32s);
    }
    if (png) {
        png_destroy_read_struct(&png, &info, NULL);
    }

    fclose(reader);

    return image;

}/* pngtoimage() */