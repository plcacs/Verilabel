void dostor(char *name, const int append, const int autorename)
{
    ULHandler ulhandler;
    int f;
    const char *ul_name = NULL;
    const char *atomic_file = NULL;
    off_t filesize = (off_t) 0U;
    struct stat st;
    double started = 0.0;
    signed char overwrite = 0;
    int overflow = 0;
    int ret = -1;
    off_t max_filesize = (off_t) -1;
#ifdef QUOTAS
    Quota quota;
#endif
    const char *name2 = NULL;

    if (type < 1 || (type == 1 && restartat > (off_t) 1)) {
        addreply_noformat(503, MSG_NO_ASCII_RESUME);
        goto end;
    }
#ifndef ANON_CAN_RESUME
    if (guest != 0 && anon_noupload != 0) {
        addreply_noformat(550, MSG_ANON_CANT_OVERWRITE);
        goto end;
    }
#endif
    if (ul_check_free_space(name, -1.0) == 0) {
        addreply_noformat(552, MSG_NO_DISK_SPACE);
        goto end;
    }
    if (checknamesanity(name, dot_write_ok) != 0) {
        addreply(553, MSG_SANITY_FILE_FAILURE, name);
        goto end;
    }
    if (autorename != 0) {
        no_truncate = 1;
    }
    if (restartat > (off_t) 0 || no_truncate != 0) {
        if ((atomic_file = get_atomic_file(name)) == NULL) {
            addreply(553, MSG_SANITY_FILE_FAILURE, name);
            goto end;
        }
        if (restartat > (off_t) 0 &&
            rename(name, atomic_file) != 0 && errno != ENOENT) {
            error(553, MSG_RENAME_FAILURE);
            atomic_file = NULL;
            goto end;
        }
    }
    if (atomic_file != NULL) {
        ul_name = atomic_file;
    } else {
        ul_name = name;
    }
    if (atomic_file == NULL &&
        (f = open(ul_name, O_WRONLY | O_NOFOLLOW)) != -1) {
        overwrite++;
    } else if ((f = open(ul_name, O_CREAT | O_WRONLY | O_NOFOLLOW,
                         (mode_t) 0777 & ~u_mask)) == -1) {
        error(553, MSG_OPEN_FAILURE2);
        goto end;
    }
    if (fstat(f, &st) < 0) {
        (void) close(f);
        error(553, MSG_STAT_FAILURE2);
        goto end;
    }
    if (!S_ISREG(st.st_mode)) {
        (void) close(f);
        addreply_noformat(550, MSG_NOT_REGULAR_FILE);
        goto end;
    }
    alarm(MAX_SESSION_XFER_IDLE);

    /* Anonymous users *CAN* overwrite 0-bytes files - This is the right behavior */
    if (st.st_size > (off_t) 0) {
#ifndef ANON_CAN_RESUME
        if (guest != 0) {
            addreply_noformat(550, MSG_ANON_CANT_OVERWRITE);
            (void) close(f);
            goto end;
        }
#endif
        if (append != 0) {
            restartat = st.st_size;
        }
    } else {
        restartat = (off_t) 0;
    }
    if (restartat > st.st_size) {
        restartat = st.st_size;
    }
    if (restartat > (off_t) 0 && lseek(f, restartat, SEEK_SET) < (off_t) 0) {
        (void) close(f);
        error(451, "seek");
        goto end;
    }
    if (restartat < st.st_size) {
        if (ftruncate(f, restartat) < 0) {
            (void) close(f);
            error(451, "ftruncate");
            goto end;
        }
#ifdef QUOTAS
        if (restartat != st.st_size) {
            (void) quota_update(NULL, 0LL,
                                (long long) (restartat - st.st_size),
                                &overflow);
        }
#endif
    }
#ifdef QUOTAS
    if (quota_update(&quota, 0LL, 0LL, &overflow) == 0 &&
        (overflow > 0 || quota.files >= user_quota_files ||
         quota.size > user_quota_size ||
         (max_filesize >= (off_t) 0 &&
          (max_filesize = user_quota_size - quota.size) < (off_t) 0))) {
        overflow = 1;
        (void) close(f);
        goto afterquota;
    }
#endif
    opendata();
    if (xferfd == -1) {
        (void) close(f);
        goto end;
    }
    doreply();
# ifdef WITH_TLS
    if (data_protection_level == CPL_PRIVATE) {
        tls_init_data_session(xferfd, passive);
    }
# endif
    state_needs_update = 1;
    setprocessname("pure-ftpd (UPLOAD)");
    filesize = restartat;

#ifdef FTPWHO
    if (shm_data_cur != NULL) {
        const size_t sl = strlen(name);

        ftpwho_lock();
        shm_data_cur->state = FTPWHO_STATE_UPLOAD;
        shm_data_cur->download_total_size = (off_t) 0U;
        shm_data_cur->download_current_size = (off_t) filesize;
        shm_data_cur->restartat = restartat;
        (void) time(&shm_data_cur->xfer_date);
        if (sl < sizeof shm_data_cur->filename) {
            memcpy(shm_data_cur->filename, name, sl);
            shm_data_cur->filename[sl] = 0;
        } else {
            memcpy(shm_data_cur->filename,
                   &name[sl - sizeof shm_data_cur->filename - 1U],
                   sizeof shm_data_cur->filename);
        }
        ftpwho_unlock();
    }
#endif

    /* Here starts the real upload code */

    started = get_usec_time();

    if (ul_init(&ulhandler, clientfd, tls_cnx, xferfd, name, f, tls_data_cnx,
                restartat, type == 1, throttling_bandwidth_ul,
                max_filesize) == 0) {
        ret = ul_send(&ulhandler);
        ul_exit(&ulhandler);
    } else {
        ret = -1;
    }
    (void) close(f);
    closedata();

    /* Here ends the real upload code */

#ifdef SHOW_REAL_DISK_SPACE
    if (FSTATFS(f, &statfsbuf) == 0) {
        double space;

        space = (double) STATFS_BAVAIL(statfsbuf) *
            (double) STATFS_FRSIZE(statfsbuf);
        if (space > 524288.0) {
            addreply(0, MSG_SPACE_FREE_M, space / 1048576.0);
        } else {
            addreply(0, MSG_SPACE_FREE_K, space / 1024.0);
        }
    }
#endif

    uploaded += (unsigned long long) ulhandler.total_uploaded;
    {
        off_t atomic_file_size;
        off_t original_file_size;
        int files_count;

        if (overwrite == 0) {
            files_count = 1;
        } else {
            files_count = 0;
        }
        if (autorename != 0 && restartat == (off_t) 0) {
            if ((atomic_file_size = get_file_size(atomic_file)) < (off_t) 0) {
                goto afterquota;
            }
            if (tryautorename(atomic_file, name, &name2) != 0) {
                error(553, MSG_RENAME_FAILURE);
                goto afterquota;
            } else {
#ifdef QUOTAS
                ul_quota_update(name2 ? name2 : name, 1, atomic_file_size);
#endif
                atomic_file = NULL;
            }
        } else if (atomic_file != NULL) {
            if ((atomic_file_size = get_file_size(atomic_file)) < (off_t) 0) {
                goto afterquota;
            }
            if ((original_file_size = get_file_size(name)) < (off_t) 0 ||
                restartat > original_file_size) {
                original_file_size = restartat;
            }
            if (rename(atomic_file, name) != 0) {
                error(553, MSG_RENAME_FAILURE);
                goto afterquota;
            } else {
#ifdef QUOTAS
                overflow = ul_quota_update
                    (name, files_count, atomic_file_size - original_file_size);
#endif
                atomic_file = NULL;
            }
        } else {
#ifdef QUOTAS
            overflow = ul_quota_update
                (name, files_count, ulhandler.total_uploaded);
#endif
        }
    }
    afterquota:
    if (overflow > 0) {
        addreply(552, MSG_QUOTA_EXCEEDED, name);
    } else {
        if (ret == 0) {
            addreply_noformat(226, MSG_TRANSFER_SUCCESSFUL);
        } else {
            addreply_noformat(451, MSG_ABORTED);
        }
        displayrate(MSG_UPLOADED, ulhandler.total_uploaded, started,
                    name2 ? name2 : name, 1);
    }
    end:
    restartat = (off_t) 0;
    if (atomic_file != NULL) {
        unlink(atomic_file);
        atomic_file = NULL;
    }
}