static int track_header(VividasDemuxContext *viv, AVFormatContext *s,  uint8_t *buf, int size)
{
    int i, j, ret;
    int64_t off;
    int val_1;
    int num_video;
    AVIOContext pb0, *pb = &pb0;

    ffio_init_context(pb, buf, size, 0, NULL, NULL, NULL, NULL);

    ffio_read_varlen(pb); // track_header_len
    avio_r8(pb); // '1'

    val_1 = ffio_read_varlen(pb);

    for (i=0;i<val_1;i++) {
        int c = avio_r8(pb);
        if (avio_feof(pb))
            return AVERROR_EOF;
        for (j=0;j<c;j++) {
            if (avio_feof(pb))
                return AVERROR_EOF;
            avio_r8(pb); // val_3
            avio_r8(pb); // val_4
        }
    }

    avio_r8(pb); // num_streams

    off = avio_tell(pb);
    off += ffio_read_varlen(pb); // val_5

    avio_r8(pb); // '2'
    num_video = avio_r8(pb);

    avio_seek(pb, off, SEEK_SET);
    if (num_video != 1) {
        av_log(s, AV_LOG_ERROR, "number of video tracks %d is not 1\n", num_video);
        return AVERROR_PATCHWELCOME;
    }

    for (i = 0; i < num_video; i++) {
        AVStream *st = avformat_new_stream(s, NULL);
        int num, den;

        if (!st)
            return AVERROR(ENOMEM);

        st->id = i;

        st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        st->codecpar->codec_id = AV_CODEC_ID_VP6;

        off = avio_tell(pb);
        off += ffio_read_varlen(pb);
        avio_r8(pb); // '3'
        avio_r8(pb); // val_7
        num = avio_rl32(pb); // frame_time
        den = avio_rl32(pb); // time_base
        avpriv_set_pts_info(st, 64, num, den);
        st->nb_frames = avio_rl32(pb); // n frames
        st->codecpar->width = avio_rl16(pb); // width
        st->codecpar->height = avio_rl16(pb); // height
        avio_r8(pb); // val_8
        avio_rl32(pb); // val_9

        avio_seek(pb, off, SEEK_SET);
    }

    off = avio_tell(pb);
    off += ffio_read_varlen(pb); // val_10
    avio_r8(pb); // '4'
    viv->num_audio = avio_r8(pb);
    avio_seek(pb, off, SEEK_SET);

    if (viv->num_audio != 1)
        av_log(s, AV_LOG_WARNING, "number of audio tracks %d is not 1\n", viv->num_audio);

    for(i=0;i<viv->num_audio;i++) {
        int q;
        AVStream *st = avformat_new_stream(s, NULL);
        if (!st)
            return AVERROR(ENOMEM);

        st->id = num_video + i;

        st->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        st->codecpar->codec_id = AV_CODEC_ID_VORBIS;

        off = avio_tell(pb);
        off += ffio_read_varlen(pb); // length
        avio_r8(pb); // '5'
        avio_r8(pb); //codec_id
        avio_rl16(pb); //codec_subid
        st->codecpar->channels = avio_rl16(pb); // channels
        st->codecpar->sample_rate = avio_rl32(pb); // sample_rate
        avio_seek(pb, 10, SEEK_CUR); // data_1
        q = avio_r8(pb);
        avio_seek(pb, q, SEEK_CUR); // data_2
        avio_r8(pb); // zeropad

        if (avio_tell(pb) < off) {
            int num_data;
            int xd_size = 0;
            int data_len[256];
            int offset = 1;
            uint8_t *p;
            ffio_read_varlen(pb); // val_13
            avio_r8(pb); // '19'
            ffio_read_varlen(pb); // len_3
            num_data = avio_r8(pb);
            for (j = 0; j < num_data; j++) {
                uint64_t len = ffio_read_varlen(pb);
                if (len > INT_MAX/2 - xd_size) {
                    return AVERROR_INVALIDDATA;
                }
                data_len[j] = len;
                xd_size += len;
            }

            ret = ff_alloc_extradata(st->codecpar, 64 + xd_size + xd_size / 255);
            if (ret < 0)
                return ret;

            p = st->codecpar->extradata;
            p[0] = 2;

            for (j = 0; j < num_data - 1; j++) {
                unsigned delta = av_xiphlacing(&p[offset], data_len[j]);
                if (delta > data_len[j]) {
                    return AVERROR_INVALIDDATA;
                }
                offset += delta;
            }

            for (j = 0; j < num_data; j++) {
                int ret = avio_read(pb, &p[offset], data_len[j]);
                if (ret < data_len[j]) {
                    st->codecpar->extradata_size = 0;
                    av_freep(&st->codecpar->extradata);
                    break;
                }
                offset += data_len[j];
            }

            if (offset < st->codecpar->extradata_size)
                st->codecpar->extradata_size = offset;
        }
    }

    return 0;
}