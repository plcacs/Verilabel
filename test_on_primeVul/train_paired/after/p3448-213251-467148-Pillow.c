j2k_decode_entry(Imaging im, ImagingCodecState state) {
    JPEG2KDECODESTATE *context = (JPEG2KDECODESTATE *)state->context;
    opj_stream_t *stream = NULL;
    opj_image_t *image = NULL;
    opj_codec_t *codec = NULL;
    opj_dparameters_t params;
    OPJ_COLOR_SPACE color_space;
    j2k_unpacker_t unpack = NULL;
    size_t buffer_size = 0, tile_bytes = 0;
    unsigned n, tile_height, tile_width;
    int total_component_width = 0;

    stream = opj_stream_create(BUFFER_SIZE, OPJ_TRUE);

    if (!stream) {
        state->errcode = IMAGING_CODEC_BROKEN;
        state->state = J2K_STATE_FAILED;
        goto quick_exit;
    }

    opj_stream_set_read_function(stream, j2k_read);
    opj_stream_set_skip_function(stream, j2k_skip);

    /* OpenJPEG 2.0 doesn't have OPJ_VERSION_MAJOR */
#ifndef OPJ_VERSION_MAJOR
    opj_stream_set_user_data(stream, state);
#else
    opj_stream_set_user_data(stream, state, NULL);

    /* Hack: if we don't know the length, the largest file we can
       possibly support is 4GB.  We can't go larger than this, because
       OpenJPEG truncates this value for the final box in the file, and
       the box lengths in OpenJPEG are currently 32 bit. */
    if (context->length < 0) {
        opj_stream_set_user_data_length(stream, 0xffffffff);
    } else {
        opj_stream_set_user_data_length(stream, context->length);
    }
#endif

    /* Setup decompression context */
    context->error_msg = NULL;

    opj_set_default_decoder_parameters(&params);
    params.cp_reduce = context->reduce;
    params.cp_layer = context->layers;

    codec = opj_create_decompress(context->format);

    if (!codec) {
        state->errcode = IMAGING_CODEC_BROKEN;
        state->state = J2K_STATE_FAILED;
        goto quick_exit;
    }

    opj_set_error_handler(codec, j2k_error, context);
    opj_setup_decoder(codec, &params);

    if (!opj_read_header(stream, codec, &image)) {
        state->errcode = IMAGING_CODEC_BROKEN;
        state->state = J2K_STATE_FAILED;
        goto quick_exit;
    }

    /* Check that this image is something we can handle */
    if (image->numcomps < 1 || image->numcomps > 4 ||
        image->color_space == OPJ_CLRSPC_UNKNOWN) {
        state->errcode = IMAGING_CODEC_BROKEN;
        state->state = J2K_STATE_FAILED;
        goto quick_exit;
    }

    for (n = 1; n < image->numcomps; ++n) {
        if (image->comps[n].dx != 1 || image->comps[n].dy != 1) {
            state->errcode = IMAGING_CODEC_BROKEN;
            state->state = J2K_STATE_FAILED;
            goto quick_exit;
        }
    }

    /*
         Colorspace    Number of components    PIL mode
       ------------------------------------------------------
         sRGB          3                       RGB
         sRGB          4                       RGBA
         gray          1                       L or I
         gray          2                       LA
         YCC           3                       YCbCr


       If colorspace is unspecified, we assume:

           Number of components   Colorspace
         -----------------------------------------
           1                      gray
           2                      gray (+ alpha)
           3                      sRGB
           4                      sRGB (+ alpha)

    */

    /* Find the correct unpacker */
    color_space = image->color_space;

    if (color_space == OPJ_CLRSPC_UNSPECIFIED) {
        switch (image->numcomps) {
            case 1:
            case 2:
                color_space = OPJ_CLRSPC_GRAY;
                break;
            case 3:
            case 4:
                color_space = OPJ_CLRSPC_SRGB;
                break;
        }
    }

    for (n = 0; n < sizeof(j2k_unpackers) / sizeof(j2k_unpackers[0]); ++n) {
        if (color_space == j2k_unpackers[n].color_space &&
            image->numcomps == j2k_unpackers[n].components &&
            strcmp(im->mode, j2k_unpackers[n].mode) == 0) {
            unpack = j2k_unpackers[n].unpacker;
            break;
        }
    }

    if (!unpack) {
        state->errcode = IMAGING_CODEC_BROKEN;
        state->state = J2K_STATE_FAILED;
        goto quick_exit;
    }

    /* Decode the image tile-by-tile; this means we only need use as much
       memory as is required for one tile's worth of components. */
    for (;;) {
        JPEG2KTILEINFO tile_info;
        OPJ_BOOL should_continue;
        unsigned correction = (1 << params.cp_reduce) - 1;

        if (!opj_read_tile_header(
                codec,
                stream,
                &tile_info.tile_index,
                &tile_info.data_size,
                &tile_info.x0,
                &tile_info.y0,
                &tile_info.x1,
                &tile_info.y1,
                &tile_info.nb_comps,
                &should_continue)) {
            state->errcode = IMAGING_CODEC_BROKEN;
            state->state = J2K_STATE_FAILED;
            goto quick_exit;
        }

        if (!should_continue) {
            break;
        }

        /* Adjust the tile co-ordinates based on the reduction (OpenJPEG
           doesn't do this for us) */
        tile_info.x0 = (tile_info.x0 + correction) >> context->reduce;
        tile_info.y0 = (tile_info.y0 + correction) >> context->reduce;
        tile_info.x1 = (tile_info.x1 + correction) >> context->reduce;
        tile_info.y1 = (tile_info.y1 + correction) >> context->reduce;

        /* Check the tile bounds; if the tile is outside the image area,
           or if it has a negative width or height (i.e. the coordinates are
           swapped), bail. */
        if (tile_info.x0 >= tile_info.x1 || tile_info.y0 >= tile_info.y1 ||
            tile_info.x0 < 0 || tile_info.y0 < 0 ||
            (OPJ_UINT32)tile_info.x0 < image->x0 ||
            (OPJ_UINT32)tile_info.y0 < image->y0 ||
            (OPJ_INT32)(tile_info.x1 - image->x0) > im->xsize ||
            (OPJ_INT32)(tile_info.y1 - image->y0) > im->ysize) {
            state->errcode = IMAGING_CODEC_BROKEN;
            state->state = J2K_STATE_FAILED;
            goto quick_exit;
        }

        if (tile_info.nb_comps != image->numcomps) {
            state->errcode = IMAGING_CODEC_BROKEN;
            state->state = J2K_STATE_FAILED;
            goto quick_exit;
        }

        /* Sometimes the tile_info.datasize we get back from openjpeg
           is less than sum(comp_bytes)*w*h, and we overflow in the
           shuffle stage */

        tile_width = tile_info.x1 - tile_info.x0;
        tile_height = tile_info.y1 - tile_info.y0;

        /* Total component width = sum (component_width) e.g, it's
         legal for an la file to have a 1 byte width for l, and 4 for
         a. and then a malicious file could have a smaller tile_bytes
        */

        for (n=0; n < tile_info.nb_comps; n++) {
            // see csize /acsize calcs
            int csize = (image->comps[n].prec + 7) >> 3;
            csize = (csize == 3) ? 4 : csize;
            total_component_width += csize;
        }
        if ((tile_width > UINT_MAX / total_component_width) ||
            (tile_height > UINT_MAX / total_component_width) ||
            (tile_width > UINT_MAX / (tile_height * total_component_width)) ||
            (tile_height > UINT_MAX / (tile_width * total_component_width))) {
            state->errcode = IMAGING_CODEC_BROKEN;
            state->state = J2K_STATE_FAILED;
            goto quick_exit;
        }

        tile_bytes = tile_width * tile_height * total_component_width;

        if (tile_bytes > tile_info.data_size) {
            tile_info.data_size = tile_bytes;
        }

        if (buffer_size < tile_info.data_size) {
            /* malloc check ok, overflow and tile size sanity check above */
            UINT8 *new = realloc(state->buffer, tile_info.data_size);
            if (!new) {
                state->errcode = IMAGING_CODEC_MEMORY;
                state->state = J2K_STATE_FAILED;
                goto quick_exit;
            }
            state->buffer = new;
            buffer_size = tile_info.data_size;
        }

        if (!opj_decode_tile_data(
                codec,
                tile_info.tile_index,
                (OPJ_BYTE *)state->buffer,
                tile_info.data_size,
                stream)) {
            state->errcode = IMAGING_CODEC_BROKEN;
            state->state = J2K_STATE_FAILED;
            goto quick_exit;
        }

        unpack(image, &tile_info, state->buffer, im);
    }

    if (!opj_end_decompress(codec, stream)) {
        state->errcode = IMAGING_CODEC_BROKEN;
        state->state = J2K_STATE_FAILED;
        goto quick_exit;
    }

    state->state = J2K_STATE_DONE;
    state->errcode = IMAGING_CODEC_END;

    if (context->pfile) {
        if (fclose(context->pfile)) {
            context->pfile = NULL;
        }
    }

quick_exit:
    if (codec) {
        opj_destroy_codec(codec);
    }
    if (image) {
        opj_image_destroy(image);
    }
    if (stream) {
        opj_stream_destroy(stream);
    }

    return -1;
}