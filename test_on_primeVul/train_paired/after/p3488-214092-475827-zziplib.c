zzip_disk_entry_fopen(ZZIP_DISK * disk, ZZIP_DISK_ENTRY * entry)
{
    /* keep this in sync with zzip_mem_entry_fopen */
    struct zzip_file_header *header =
        zzip_disk_entry_to_file_header(disk, entry);
    if (! header)
        return 0; /* EBADMSG */
    ___ ZZIP_DISK_FILE *file = malloc(sizeof(ZZIP_DISK_FILE));
    if (! file)
        return 0; /* ENOMEM */
    file->buffer = disk->buffer;
    file->endbuf = disk->endbuf;
    file->avail = zzip_file_header_usize(header);

    if (! file->avail || zzip_file_header_data_stored(header))
    { 
         file->stored = zzip_file_header_to_data (header);
         DBG2("stored size %i", (int) file->avail);
         if (file->stored + file->avail >= disk->endbuf)
             goto error;
         return file; 
    }

    file->stored = 0;
    file->zlib.opaque = 0;
    file->zlib.zalloc = Z_NULL;
    file->zlib.zfree = Z_NULL;
    file->zlib.avail_in = zzip_file_header_csize(header);
    file->zlib.next_in = zzip_file_header_to_data(header);

    DBG2("compressed size %i", (int) file->zlib.avail_in);
    if (file->zlib.next_in + file->zlib.avail_in >= disk->endbuf)
         goto error;
    if (file->zlib.next_in < disk->buffer)
         goto error;

    if (! zzip_file_header_data_deflated(header))
        goto error;
    if (inflateInit2(&file->zlib, -MAX_WBITS) != Z_OK)
        goto error;

    return file;
error:
    free (file);
    errno = EBADMSG;
    return 0; 
    ____;
}