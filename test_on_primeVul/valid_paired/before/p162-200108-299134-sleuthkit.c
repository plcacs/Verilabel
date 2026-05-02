    yaffsfs_istat(TSK_FS_INFO *fs, TSK_FS_ISTAT_FLAG_ENUM flags, FILE * hFile, TSK_INUM_T inum,
    TSK_DADDR_T numblock, int32_t sec_skew)
{
    TSK_FS_META *fs_meta;
    TSK_FS_FILE *fs_file;
    YAFFSFS_INFO *yfs = (YAFFSFS_INFO *)fs;
    char ls[12];
    YAFFSFS_PRINT_ADDR print;
    char timeBuf[32];
    YaffsCacheObject * obj = NULL;
    YaffsCacheVersion * version = NULL;
    YaffsHeader * header = NULL;

    yaffscache_version_find_by_inode(yfs, inum, &version, &obj);

    if ((fs_file = tsk_fs_file_open_meta(fs, NULL, inum)) == NULL) {
        return 1;
    }
    fs_meta = fs_file->meta;

    tsk_fprintf(hFile, "inode: %" PRIuINUM "\n", inum);
    tsk_fprintf(hFile, "%sAllocated\n",
        (fs_meta->flags & TSK_FS_META_FLAG_ALLOC) ? "" : "Not ");

    if (fs_meta->link)
        tsk_fprintf(hFile, "symbolic link to: %s\n", fs_meta->link);

    tsk_fprintf(hFile, "uid / gid: %" PRIuUID " / %" PRIuGID "\n",
        fs_meta->uid, fs_meta->gid);

    tsk_fs_meta_make_ls(fs_meta, ls, sizeof(ls));
    tsk_fprintf(hFile, "mode: %s\n", ls);

    tsk_fprintf(hFile, "size: %" PRIdOFF "\n", fs_meta->size);
    tsk_fprintf(hFile, "num of links: %d\n", fs_meta->nlink);

    if(version != NULL){
        yaffsfs_read_header(yfs, &header, version->ycv_header_chunk->ycc_offset);
        if(header != NULL){
            tsk_fprintf(hFile, "Name: %s\n", header->name);
        }
    }

    if (sec_skew != 0) {
        tsk_fprintf(hFile, "\nAdjusted Inode Times:\n");
        fs_meta->mtime -= sec_skew;
        fs_meta->atime -= sec_skew;
        fs_meta->ctime -= sec_skew;

        tsk_fprintf(hFile, "Accessed:\t%s\n",
            tsk_fs_time_to_str(fs_meta->atime, timeBuf));
        tsk_fprintf(hFile, "File Modified:\t%s\n",
            tsk_fs_time_to_str(fs_meta->mtime, timeBuf));
        tsk_fprintf(hFile, "Inode Modified:\t%s\n",
            tsk_fs_time_to_str(fs_meta->ctime, timeBuf));

        fs_meta->mtime += sec_skew;
        fs_meta->atime += sec_skew;
        fs_meta->ctime += sec_skew;

        tsk_fprintf(hFile, "\nOriginal Inode Times:\n");
    }
    else {
        tsk_fprintf(hFile, "\nInode Times:\n");
    }

    tsk_fprintf(hFile, "Accessed:\t%s\n",
        tsk_fs_time_to_str(fs_meta->atime, timeBuf));
    tsk_fprintf(hFile, "File Modified:\t%s\n",
        tsk_fs_time_to_str(fs_meta->mtime, timeBuf));
    tsk_fprintf(hFile, "Inode Modified:\t%s\n",
        tsk_fs_time_to_str(fs_meta->ctime, timeBuf));

    if(version != NULL){
        tsk_fprintf(hFile, "\nHeader Chunk:\n");
        tsk_fprintf(hFile, "%" PRIuDADDR "\n", (version->ycv_header_chunk->ycc_offset / (yfs->page_size + yfs->spare_size)));
    }

    if (numblock > 0) {
        TSK_OFF_T lower_size = numblock * fs->block_size;
        fs_meta->size = (lower_size < fs_meta->size)?(lower_size):(fs_meta->size);
    }
    tsk_fprintf(hFile, "\nData Chunks:\n");


    if (flags & TSK_FS_ISTAT_RUNLIST){
        const TSK_FS_ATTR *fs_attr_default =
            tsk_fs_file_attr_get_type(fs_file,
                TSK_FS_ATTR_TYPE_DEFAULT, 0, 0);
        if (fs_attr_default && (fs_attr_default->flags & TSK_FS_ATTR_NONRES)) {
            if (tsk_fs_attr_print(fs_attr_default, hFile)) {
                tsk_fprintf(hFile, "\nError creating run lists  ");
                tsk_error_print(hFile);
                tsk_error_reset();
            }
        }
    }
    else {
        print.idx = 0;
        print.hFile = hFile;

        if (tsk_fs_file_walk(fs_file, TSK_FS_FILE_WALK_FLAG_AONLY,
            (TSK_FS_FILE_WALK_CB)print_addr_act, (void *)&print)) {
            tsk_fprintf(hFile, "\nError reading file:  ");
            tsk_error_print(hFile);
            tsk_error_reset();
        }
        else if (print.idx != 0) {
            tsk_fprintf(hFile, "\n");
        }
    }

    tsk_fs_file_close(fs_file);

    return 0;
}