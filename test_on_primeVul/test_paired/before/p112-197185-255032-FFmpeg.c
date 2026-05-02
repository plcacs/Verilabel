static int adts_decode_extradata(AVFormatContext *s, ADTSContext *adts, const uint8_t *buf, int size)
{
    GetBitContext gb;
    PutBitContext pb;
    MPEG4AudioConfig m4ac;
    int off;

    init_get_bits(&gb, buf, size * 8);
    off = avpriv_mpeg4audio_get_config2(&m4ac, buf, size, 1, s);
    if (off < 0)
        return off;
    skip_bits_long(&gb, off);
    adts->objecttype        = m4ac.object_type - 1;
    adts->sample_rate_index = m4ac.sampling_index;
    adts->channel_conf      = m4ac.chan_config;

    if (adts->objecttype > 3U) {
        av_log(s, AV_LOG_ERROR, "MPEG-4 AOT %d is not allowed in ADTS\n", adts->objecttype+1);
        return AVERROR_INVALIDDATA;
    }
    if (adts->sample_rate_index == 15) {
        av_log(s, AV_LOG_ERROR, "Escape sample rate index illegal in ADTS\n");
        return AVERROR_INVALIDDATA;
    }
    if (get_bits(&gb, 1)) {
        av_log(s, AV_LOG_ERROR, "960/120 MDCT window is not allowed in ADTS\n");
        return AVERROR_INVALIDDATA;
    }
    if (get_bits(&gb, 1)) {
        av_log(s, AV_LOG_ERROR, "Scalable configurations are not allowed in ADTS\n");
        return AVERROR_INVALIDDATA;
    }
    if (get_bits(&gb, 1)) {
        av_log(s, AV_LOG_ERROR, "Extension flag is not allowed in ADTS\n");
        return AVERROR_INVALIDDATA;
    }
    if (!adts->channel_conf) {
        init_put_bits(&pb, adts->pce_data, MAX_PCE_SIZE);

        put_bits(&pb, 3, 5); //ID_PCE
        adts->pce_size = (ff_copy_pce_data(&pb, &gb) + 3) / 8;
        flush_put_bits(&pb);
    }

    adts->write_adts = 1;

    return 0;
}