prepare_repo_download_targets(LrHandle *handle,
                              LrYumRepo *repo,
                              LrYumRepoMd *repomd,
                              LrMetadataTarget *mdtarget,
                              GSList **targets,
                              GSList **cbdata_list,
                              GError **err)
{
    char *destdir;  /* Destination dir */

    destdir = handle->destdir;
    assert(destdir);
    assert(strlen(destdir));
    assert(!err || *err == NULL);

    if(handle->cachedir) {
        lr_yum_switch_to_zchunk(handle, repomd);
        repo->use_zchunk = TRUE;
    } else {
        g_debug("%s: Cache directory not set, disabling zchunk", __func__);
        repo->use_zchunk = FALSE;
    }

    for (GSList *elem = repomd->records; elem; elem = g_slist_next(elem)) {
        int fd;
        char *path;
        LrDownloadTarget *target;
        LrYumRepoMdRecord *record = elem->data;
        CbData *cbdata = NULL;
        void *user_cbdata = NULL;
        LrEndCb endcb = NULL;

        if (mdtarget != NULL) {
            user_cbdata = mdtarget->cbdata;
            endcb = mdtarget->endcb;
        }

        assert(record);

        if (!lr_yum_repomd_record_enabled(handle, record->type, repomd->records))
            continue;

        char *location_href = record->location_href;
        gboolean is_zchunk = FALSE;
        #ifdef WITH_ZCHUNK
        if (handle->cachedir && record->header_checksum)
            is_zchunk = TRUE;
        #endif /* WITH_ZCHUNK */

        GSList *checksums = NULL;
        if (is_zchunk) {
            #ifdef WITH_ZCHUNK
            if(!prepare_repo_download_zck_target(handle, record, &path, &fd,
                                                 &checksums, targets, err))
                return FALSE;
            #endif /* WITH_ZCHUNK */
        } else {
            if(!prepare_repo_download_std_target(handle, record, &path, &fd,
                                                 &checksums, targets, err))
                return FALSE;
        }

        if (handle->user_cb || handle->hmfcb) {
            cbdata = cbdata_new(handle->user_data,
                                user_cbdata,
                                handle->user_cb,
                                handle->hmfcb,
                                record->type);
            *cbdata_list = g_slist_append(*cbdata_list, cbdata);
        }

        target = lr_downloadtarget_new(handle,
                                       location_href,
                                       record->location_base,
                                       fd,
                                       NULL,
                                       checksums,
                                       0,
                                       0,
                                       NULL,
                                       cbdata,
                                       endcb,
                                       NULL,
                                       NULL,
                                       0,
                                       0,
                                       NULL,
                                       FALSE,
                                       is_zchunk);

        if(is_zchunk) {
            #ifdef WITH_ZCHUNK
            target->expectedsize = record->size_header;
            target->zck_header_size = record->size_header;
            #endif /* WITH_ZCHUNK */
        }

        if (mdtarget != NULL)
            mdtarget->repomd_records_to_download++;
        *targets = g_slist_append(*targets, target);

        /* Because path may already exists in repo (while update) */
        lr_yum_repo_update(repo, record->type, path);
        lr_free(path);
    }

    return TRUE;
}