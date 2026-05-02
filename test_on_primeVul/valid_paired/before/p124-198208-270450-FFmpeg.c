static int parse_playlist(HLSContext *c, const char *url,
                          struct playlist *pls, AVIOContext *in)
{
    int ret = 0, is_segment = 0, is_variant = 0;
    int64_t duration = 0;
    enum KeyType key_type = KEY_NONE;
    uint8_t iv[16] = "";
    int has_iv = 0;
    char key[MAX_URL_SIZE] = "";
    char line[MAX_URL_SIZE];
    const char *ptr;
    int close_in = 0;
    int64_t seg_offset = 0;
    int64_t seg_size = -1;
    uint8_t *new_url = NULL;
    struct variant_info variant_info;
    char tmp_str[MAX_URL_SIZE];
    struct segment *cur_init_section = NULL;

    if (!in) {
#if 1
        AVDictionary *opts = NULL;
        close_in = 1;
        /* Some HLS servers don't like being sent the range header */
        av_dict_set(&opts, "seekable", "0", 0);

        // broker prior HTTP options that should be consistent across requests
        av_dict_set(&opts, "user-agent", c->user_agent, 0);
        av_dict_set(&opts, "cookies", c->cookies, 0);
        av_dict_set(&opts, "headers", c->headers, 0);

        ret = avio_open2(&in, url, AVIO_FLAG_READ,
                         c->interrupt_callback, &opts);
        av_dict_free(&opts);
        if (ret < 0)
            return ret;
#else
        ret = open_in(c, &in, url);
        if (ret < 0)
            return ret;
        close_in = 1;
#endif
    }

    if (av_opt_get(in, "location", AV_OPT_SEARCH_CHILDREN, &new_url) >= 0)
        url = new_url;

    read_chomp_line(in, line, sizeof(line));
    if (strcmp(line, "#EXTM3U")) {
        ret = AVERROR_INVALIDDATA;
        goto fail;
    }

    if (pls) {
        free_segment_list(pls);
        pls->finished = 0;
        pls->type = PLS_TYPE_UNSPECIFIED;
    }
    while (!avio_feof(in)) {
        read_chomp_line(in, line, sizeof(line));
        if (av_strstart(line, "#EXT-X-STREAM-INF:", &ptr)) {
            is_variant = 1;
            memset(&variant_info, 0, sizeof(variant_info));
            ff_parse_key_value(ptr, (ff_parse_key_val_cb) handle_variant_args,
                               &variant_info);
        } else if (av_strstart(line, "#EXT-X-KEY:", &ptr)) {
            struct key_info info = {{0}};
            ff_parse_key_value(ptr, (ff_parse_key_val_cb) handle_key_args,
                               &info);
            key_type = KEY_NONE;
            has_iv = 0;
            if (!strcmp(info.method, "AES-128"))
                key_type = KEY_AES_128;
            if (!strcmp(info.method, "SAMPLE-AES"))
                key_type = KEY_SAMPLE_AES;
            if (!strncmp(info.iv, "0x", 2) || !strncmp(info.iv, "0X", 2)) {
                ff_hex_to_data(iv, info.iv + 2);
                has_iv = 1;
            }
            av_strlcpy(key, info.uri, sizeof(key));
        } else if (av_strstart(line, "#EXT-X-MEDIA:", &ptr)) {
            struct rendition_info info = {{0}};
            ff_parse_key_value(ptr, (ff_parse_key_val_cb) handle_rendition_args,
                               &info);
            new_rendition(c, &info, url);
        } else if (av_strstart(line, "#EXT-X-TARGETDURATION:", &ptr)) {
            ret = ensure_playlist(c, &pls, url);
            if (ret < 0)
                goto fail;
            pls->target_duration = atoi(ptr) * AV_TIME_BASE;
        } else if (av_strstart(line, "#EXT-X-MEDIA-SEQUENCE:", &ptr)) {
            ret = ensure_playlist(c, &pls, url);
            if (ret < 0)
                goto fail;
            pls->start_seq_no = atoi(ptr);
        } else if (av_strstart(line, "#EXT-X-PLAYLIST-TYPE:", &ptr)) {
            ret = ensure_playlist(c, &pls, url);
            if (ret < 0)
                goto fail;
            if (!strcmp(ptr, "EVENT"))
                pls->type = PLS_TYPE_EVENT;
            else if (!strcmp(ptr, "VOD"))
                pls->type = PLS_TYPE_VOD;
        } else if (av_strstart(line, "#EXT-X-MAP:", &ptr)) {
            struct init_section_info info = {{0}};
            ret = ensure_playlist(c, &pls, url);
            if (ret < 0)
                goto fail;
            ff_parse_key_value(ptr, (ff_parse_key_val_cb) handle_init_section_args,
                               &info);
            cur_init_section = new_init_section(pls, &info, url);
        } else if (av_strstart(line, "#EXT-X-ENDLIST", &ptr)) {
            if (pls)
                pls->finished = 1;
        } else if (av_strstart(line, "#EXTINF:", &ptr)) {
            is_segment = 1;
            duration   = atof(ptr) * AV_TIME_BASE;
        } else if (av_strstart(line, "#EXT-X-BYTERANGE:", &ptr)) {
            seg_size = atoi(ptr);
            ptr = strchr(ptr, '@');
            if (ptr)
                seg_offset = atoi(ptr+1);
        } else if (av_strstart(line, "#", NULL)) {
            continue;
        } else if (line[0]) {
            if (is_variant) {
                if (!new_variant(c, &variant_info, line, url)) {
                    ret = AVERROR(ENOMEM);
                    goto fail;
                }
                is_variant = 0;
            }
            if (is_segment) {
                struct segment *seg;
                if (!pls) {
                    if (!new_variant(c, 0, url, NULL)) {
                        ret = AVERROR(ENOMEM);
                        goto fail;
                    }
                    pls = c->playlists[c->n_playlists - 1];
                }
                seg = av_malloc(sizeof(struct segment));
                if (!seg) {
                    ret = AVERROR(ENOMEM);
                    goto fail;
                }
                seg->duration = duration;
                seg->key_type = key_type;
                if (has_iv) {
                    memcpy(seg->iv, iv, sizeof(iv));
                } else {
                    int seq = pls->start_seq_no + pls->n_segments;
                    memset(seg->iv, 0, sizeof(seg->iv));
                    AV_WB32(seg->iv + 12, seq);
                }

                if (key_type != KEY_NONE) {
                    ff_make_absolute_url(tmp_str, sizeof(tmp_str), url, key);
                    seg->key = av_strdup(tmp_str);
                    if (!seg->key) {
                        av_free(seg);
                        ret = AVERROR(ENOMEM);
                        goto fail;
                    }
                } else {
                    seg->key = NULL;
                }

                ff_make_absolute_url(tmp_str, sizeof(tmp_str), url, line);
                seg->url = av_strdup(tmp_str);
                if (!seg->url) {
                    av_free(seg->key);
                    av_free(seg);
                    ret = AVERROR(ENOMEM);
                    goto fail;
                }

                dynarray_add(&pls->segments, &pls->n_segments, seg);
                is_segment = 0;

                seg->size = seg_size;
                if (seg_size >= 0) {
                    seg->url_offset = seg_offset;
                    seg_offset += seg_size;
                    seg_size = -1;
                } else {
                    seg->url_offset = 0;
                    seg_offset = 0;
                }

                seg->init_section = cur_init_section;
            }
        }
    }
    if (pls)
        pls->last_load_time = av_gettime_relative();

fail:
    av_free(new_url);
    if (close_in)
        avio_close(in);
    return ret;
}