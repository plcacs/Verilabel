__zzip_parse_root_directory(int fd,
                            struct _disk_trailer *trailer,
                            struct zzip_dir_hdr **hdr_return,
                            zzip_plugin_io_t io)
{
    auto struct zzip_disk_entry dirent;
    struct zzip_dir_hdr *hdr;
    struct zzip_dir_hdr *hdr0;
    uint16_t *p_reclen = 0;
    zzip_off64_t entries;
    zzip_off64_t zz_offset;     /* offset from start of root directory */
    char *fd_map = 0;
    zzip_off64_t zz_fd_gap = 0;
    zzip_off64_t zz_entries = _disk_trailer_localentries(trailer);
    zzip_off64_t zz_rootsize = _disk_trailer_rootsize(trailer);
    zzip_off64_t zz_rootseek = _disk_trailer_rootseek(trailer);
    __correct_rootseek(zz_rootseek, zz_rootsize, trailer);

    if (zz_entries < 0 || zz_rootseek < 0 || zz_rootseek < 0)
        return ZZIP_CORRUPTED;

    hdr0 = (struct zzip_dir_hdr *) malloc(zz_rootsize);
    if (! hdr0)
        return ZZIP_DIRSIZE;
    hdr = hdr0;
    __debug_dir_hdr(hdr);

    if (USE_MMAP && io->fd.sys)
    {
        zz_fd_gap = zz_rootseek & (_zzip_getpagesize(io->fd.sys) - 1);
        HINT4(" fd_gap=%ld, mapseek=0x%lx, maplen=%ld", (long) (zz_fd_gap),
              (long) (zz_rootseek - zz_fd_gap),
              (long) (zz_rootsize + zz_fd_gap));
        fd_map =
            _zzip_mmap(io->fd.sys, fd, zz_rootseek - zz_fd_gap,
                       zz_rootsize + zz_fd_gap);
        /* if mmap failed we will fallback to seek/read mode */
        if (fd_map == MAP_FAILED)
        {
            NOTE2("map failed: %s", strerror(errno));
            fd_map = 0;
        } else
        {
            HINT3("mapped *%p len=%li", fd_map,
                  (long) (zz_rootsize + zz_fd_gap));
        }
    }

    for (entries=0, zz_offset=0; ; entries++)
    {
        register struct zzip_disk_entry *d;
        uint16_t u_extras, u_comment, u_namlen;

#     ifndef ZZIP_ALLOW_MODULO_ENTRIES
        if (entries >= zz_entries) {
            if (zz_offset + 256 < zz_rootsize) {
                FAIL4("%li's entry is long before the end of directory - enable modulo_entries? (O:%li R:%li)",
                      (long) entries, (long) (zz_offset), (long) zz_rootsize);
            }
            break;
        }
#     endif

        if (fd_map)
        {
            d = (void*)(fd_map+zz_fd_gap+zz_offset); /* fd_map+fd_gap==u_rootseek */
        } else
        {
            if (io->fd.seeks(fd, zz_rootseek + zz_offset, SEEK_SET) < 0)
                return ZZIP_DIR_SEEK;
            if (io->fd.read(fd, &dirent, sizeof(dirent)) < __sizeof(dirent))
                return ZZIP_DIR_READ;
            d = &dirent;
        }

        if ((zzip_off64_t) (zz_offset + sizeof(*d)) > zz_rootsize ||
            (zzip_off64_t) (zz_offset + sizeof(*d)) < 0)
        {
            FAIL4("%li's entry stretches beyond root directory (O:%li R:%li)",
                  (long) entries, (long) (zz_offset), (long) zz_rootsize);
            break;
        }

        if (! zzip_disk_entry_check_magic(d)) {
#        ifndef ZZIP_ALLOW_MODULO_ENTRIES
            FAIL4("%li's entry has no disk_entry magic indicator (O:%li R:%li)",
                  (long) entries, (long) (zz_offset), (long) zz_rootsize);
#        endif
            break;
        }

#       if 0 && defined DEBUG
        zzip_debug_xbuf((unsigned char *) d, sizeof(*d) + 8);
#       endif

        u_extras = zzip_disk_entry_get_extras(d);
        u_comment = zzip_disk_entry_get_comment(d);
        u_namlen = zzip_disk_entry_get_namlen(d);
        HINT5("offset=0x%lx, size %ld, dirent *%p, hdr %p\n",
              (long) (zz_offset + zz_rootseek), (long) zz_rootsize, d, hdr);

        /* writes over the read buffer, Since the structure where data is
           copied is smaller than the data in buffer this can be done.
           It is important that the order of setting the fields is considered
           when filling the structure, so that some data is not trashed in
           first structure read.
           at the end the whole copied list of structures  is copied into
           newly allocated buffer */
        hdr->d_crc32 = zzip_disk_entry_get_crc32(d);
        hdr->d_csize = zzip_disk_entry_get_csize(d);
        hdr->d_usize = zzip_disk_entry_get_usize(d);
        hdr->d_off = zzip_disk_entry_get_offset(d);
        hdr->d_compr = zzip_disk_entry_get_compr(d);
        if (hdr->d_compr > _255)
            hdr->d_compr = 255;

        if ((zzip_off64_t) (zz_offset + sizeof(*d) + u_namlen) > zz_rootsize ||
            (zzip_off64_t) (zz_offset + sizeof(*d) + u_namlen) < 0)
        {
            FAIL4("%li's name stretches beyond root directory (O:%li N:%li)",
                  (long) entries, (long) (zz_offset), (long) (u_namlen));
            break;
        }

        if (fd_map)
            {  memcpy(hdr->d_name, fd_map+zz_fd_gap + zz_offset+sizeof(*d), u_namlen); }
        else
            { io->fd.read(fd, hdr->d_name, u_namlen); }
        hdr->d_name[u_namlen] = '\0';
        hdr->d_namlen = u_namlen;

        /* update offset by the total length of this entry -> next entry */
        zz_offset += sizeof(*d) + u_namlen + u_extras + u_comment;

        if (zz_offset > zz_rootsize)
        {
            FAIL3("%li's entry stretches beyond root directory (O:%li)",
                  (long) entries, (long) (zz_offset));
            entries ++;
            break;
        }

        HINT5("file %ld { compr=%d crc32=$%x offset=%d",
              (long) entries, hdr->d_compr, hdr->d_crc32, hdr->d_off);
        HINT5("csize=%d usize=%d namlen=%d extras=%d",
              hdr->d_csize, hdr->d_usize, u_namlen, u_extras);
        HINT5("comment=%d name='%s' %s <sizeof %d> } ",
              u_comment, hdr->d_name, "", (int) sizeof(*d));

        p_reclen = &hdr->d_reclen;

        {
            register char *p = (char *) hdr;
            register char *q = aligned4(p + sizeof(*hdr) + u_namlen + 1);
            *p_reclen = (uint16_t) (q - p);
            hdr = (struct zzip_dir_hdr *) q;
        }
    }                           /*for */

    if (USE_MMAP && fd_map)
    {
        HINT3("unmap *%p len=%li", fd_map, (long) (zz_rootsize + zz_fd_gap));
        _zzip_munmap(io->fd.sys, fd_map, zz_rootsize + zz_fd_gap);
    }

    if (p_reclen)
    {
        *p_reclen = 0;          /* mark end of list */

        if (hdr_return)
            *hdr_return = hdr0;
    }                           /* else zero (sane) entries */
#  ifndef ZZIP_ALLOW_MODULO_ENTRIES
    return (entries != zz_entries ? ZZIP_CORRUPTED : 0);
#  else
    return ((entries & (unsigned)0xFFFF) != zz_entries ? ZZIP_CORRUPTED : 0);
#  endif
}