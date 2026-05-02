static int dwa_uncompress(EXRContext *s, const uint8_t *src, int compressed_size,
                          int uncompressed_size, EXRThreadData *td)
{
    int64_t version, lo_usize, lo_size;
    int64_t ac_size, dc_size, rle_usize, rle_csize, rle_raw_size;
    int64_t ac_count, dc_count, ac_compression;
    const int dc_w = td->xsize >> 3;
    const int dc_h = td->ysize >> 3;
    GetByteContext gb, agb;
    int skip, ret;

    if (compressed_size <= 88)
        return AVERROR_INVALIDDATA;

    version = AV_RL64(src + 0);
    if (version != 2)
        return AVERROR_INVALIDDATA;

    lo_usize = AV_RL64(src + 8);
    lo_size = AV_RL64(src + 16);
    ac_size = AV_RL64(src + 24);
    dc_size = AV_RL64(src + 32);
    rle_csize = AV_RL64(src + 40);
    rle_usize = AV_RL64(src + 48);
    rle_raw_size = AV_RL64(src + 56);
    ac_count = AV_RL64(src + 64);
    dc_count = AV_RL64(src + 72);
    ac_compression = AV_RL64(src + 80);

    if (compressed_size < 88LL + lo_size + ac_size + dc_size + rle_csize)
        return AVERROR_INVALIDDATA;

    bytestream2_init(&gb, src + 88, compressed_size - 88);
    skip = bytestream2_get_le16(&gb);
    if (skip < 2)
        return AVERROR_INVALIDDATA;

    bytestream2_skip(&gb, skip - 2);

    if (lo_size > 0) {
        if (lo_usize > uncompressed_size)
            return AVERROR_INVALIDDATA;
        bytestream2_skip(&gb, lo_size);
    }

    if (ac_size > 0) {
        unsigned long dest_len = ac_count * 2LL;
        GetByteContext agb = gb;

        if (ac_count > 3LL * td->xsize * s->scan_lines_per_block)
            return AVERROR_INVALIDDATA;

        av_fast_padded_malloc(&td->ac_data, &td->ac_size, dest_len);
        if (!td->ac_data)
            return AVERROR(ENOMEM);

        switch (ac_compression) {
        case 0:
            ret = huf_uncompress(s, td, &agb, (int16_t *)td->ac_data, ac_count);
            if (ret < 0)
                return ret;
            break;
        case 1:
            if (uncompress(td->ac_data, &dest_len, agb.buffer, ac_size) != Z_OK ||
                dest_len != ac_count * 2LL)
                return AVERROR_INVALIDDATA;
            break;
        default:
            return AVERROR_INVALIDDATA;
        }

        bytestream2_skip(&gb, ac_size);
    }

    if (dc_size > 0) {
        unsigned long dest_len = dc_count * 2LL;
        GetByteContext agb = gb;

        if (dc_count > (6LL * td->xsize * td->ysize + 63) / 64)
            return AVERROR_INVALIDDATA;

        av_fast_padded_malloc(&td->dc_data, &td->dc_size, FFALIGN(dest_len, 64) * 2);
        if (!td->dc_data)
            return AVERROR(ENOMEM);

        if (uncompress(td->dc_data + FFALIGN(dest_len, 64), &dest_len, agb.buffer, dc_size) != Z_OK ||
            (dest_len != dc_count * 2LL))
            return AVERROR_INVALIDDATA;

        s->dsp.predictor(td->dc_data + FFALIGN(dest_len, 64), dest_len);
        s->dsp.reorder_pixels(td->dc_data, td->dc_data + FFALIGN(dest_len, 64), dest_len);

        bytestream2_skip(&gb, dc_size);
    }

    if (rle_raw_size > 0 && rle_csize > 0 && rle_usize > 0) {
        unsigned long dest_len = rle_usize;

        av_fast_padded_malloc(&td->rle_data, &td->rle_size, rle_usize);
        if (!td->rle_data)
            return AVERROR(ENOMEM);

        av_fast_padded_malloc(&td->rle_raw_data, &td->rle_raw_size, rle_raw_size);
        if (!td->rle_raw_data)
            return AVERROR(ENOMEM);

        if (uncompress(td->rle_data, &dest_len, gb.buffer, rle_csize) != Z_OK ||
            (dest_len != rle_usize))
            return AVERROR_INVALIDDATA;

        ret = rle(td->rle_raw_data, td->rle_data, rle_usize, rle_raw_size);
        if (ret < 0)
            return ret;
        bytestream2_skip(&gb, rle_csize);
    }

    bytestream2_init(&agb, td->ac_data, ac_count * 2);

    for (int y = 0; y < td->ysize; y += 8) {
        for (int x = 0; x < td->xsize; x += 8) {
            memset(td->block, 0, sizeof(td->block));

            for (int j = 0; j < 3; j++) {
                float *block = td->block[j];
                const int idx = (x >> 3) + (y >> 3) * dc_w + dc_w * dc_h * j;
                uint16_t *dc = (uint16_t *)td->dc_data;
                union av_intfloat32 dc_val;

                dc_val.i = half2float(dc[idx], s->mantissatable,
                                      s->exponenttable, s->offsettable);

                block[0] = dc_val.f;
                ac_uncompress(s, &agb, block);
                dct_inverse(block);
            }

            {
                const float scale = s->pixel_type == EXR_FLOAT ? 2.f : 1.f;
                const int o = s->nb_channels == 4;
                float *bo = ((float *)td->uncompressed_data) +
                    y * td->xsize * s->nb_channels + td->xsize * (o + 0) + x;
                float *go = ((float *)td->uncompressed_data) +
                    y * td->xsize * s->nb_channels + td->xsize * (o + 1) + x;
                float *ro = ((float *)td->uncompressed_data) +
                    y * td->xsize * s->nb_channels + td->xsize * (o + 2) + x;
                float *yb = td->block[0];
                float *ub = td->block[1];
                float *vb = td->block[2];

                for (int yy = 0; yy < 8; yy++) {
                    for (int xx = 0; xx < 8; xx++) {
                        const int idx = xx + yy * 8;

                        convert(yb[idx], ub[idx], vb[idx], &bo[xx], &go[xx], &ro[xx]);

                        bo[xx] = to_linear(bo[xx], scale);
                        go[xx] = to_linear(go[xx], scale);
                        ro[xx] = to_linear(ro[xx], scale);
                    }

                    bo += td->xsize * s->nb_channels;
                    go += td->xsize * s->nb_channels;
                    ro += td->xsize * s->nb_channels;
                }
            }
        }
    }

    if (s->nb_channels < 4)
        return 0;

    for (int y = 0; y < td->ysize && td->rle_raw_data; y++) {
        uint32_t *ao = ((uint32_t *)td->uncompressed_data) + y * td->xsize * s->nb_channels;
        uint8_t *ai0 = td->rle_raw_data + y * td->xsize;
        uint8_t *ai1 = td->rle_raw_data + y * td->xsize + rle_raw_size / 2;

        for (int x = 0; x < td->xsize; x++) {
            uint16_t ha = ai0[x] | (ai1[x] << 8);

            ao[x] = half2float(ha, s->mantissatable, s->exponenttable, s->offsettable);
        }
    }

    return 0;
}