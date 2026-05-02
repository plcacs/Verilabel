hivex_open (const char *filename, int flags)
{
  hive_h *h = NULL;

  assert (sizeof (struct ntreg_header) == 0x1000);
  assert (offsetof (struct ntreg_header, csum) == 0x1fc);

  h = calloc (1, sizeof *h);
  if (h == NULL)
    goto error;

  h->msglvl = flags & HIVEX_OPEN_MSGLVL_MASK;

  const char *debug = getenv ("HIVEX_DEBUG");
  if (debug && STREQ (debug, "1"))
    h->msglvl = 2;

  DEBUG (2, "created handle %p", h);

  h->writable = !!(flags & HIVEX_OPEN_WRITE);
  h->filename = strdup (filename);
  if (h->filename == NULL)
    goto error;

#ifdef O_CLOEXEC
  h->fd = open (filename, O_RDONLY | O_CLOEXEC | O_BINARY);
#else
  h->fd = open (filename, O_RDONLY | O_BINARY);
#endif
  if (h->fd == -1)
    goto error;
#ifndef O_CLOEXEC
  fcntl (h->fd, F_SETFD, FD_CLOEXEC);
#endif

  struct stat statbuf;
  if (fstat (h->fd, &statbuf) == -1)
    goto error;

  h->size = statbuf.st_size;

  if (h->size < 0x2000) {
    SET_ERRNO (EINVAL,
               "%s: file is too small to be a Windows NT Registry hive file",
               filename);
    goto error;
  }

  if (!h->writable) {
    h->addr = mmap (NULL, h->size, PROT_READ, MAP_SHARED, h->fd, 0);
    if (h->addr == MAP_FAILED)
      goto error;

    DEBUG (2, "mapped file at %p", h->addr);
  } else {
    h->addr = malloc (h->size);
    if (h->addr == NULL)
      goto error;

    if (full_read (h->fd, h->addr, h->size) < h->size)
      goto error;

    /* We don't need the file descriptor along this path, since we
     * have read all the data.
     */
    if (close (h->fd) == -1)
      goto error;
    h->fd = -1;
  }

  /* Check header. */
  if (h->hdr->magic[0] != 'r' ||
      h->hdr->magic[1] != 'e' ||
      h->hdr->magic[2] != 'g' ||
      h->hdr->magic[3] != 'f') {
    SET_ERRNO (ENOTSUP,
               "%s: not a Windows NT Registry hive file", filename);
    goto error;
  }

  /* Check major version. */
  uint32_t major_ver = le32toh (h->hdr->major_ver);
  if (major_ver != 1) {
    SET_ERRNO (ENOTSUP,
               "%s: hive file major version %" PRIu32 " (expected 1)",
               filename, major_ver);
    goto error;
  }

  h->bitmap = calloc (1 + h->size / 32, 1);
  if (h->bitmap == NULL)
    goto error;

  /* Header checksum. */
  uint32_t sum = header_checksum (h);
  if (sum != le32toh (h->hdr->csum)) {
    SET_ERRNO (EINVAL, "%s: bad checksum in hive header", filename);
    goto error;
  }

  /* Last modified time. */
  h->last_modified = le64toh ((int64_t) h->hdr->last_modified);

  if (h->msglvl >= 2) {
    char *name = _hivex_windows_utf16_to_utf8 (h->hdr->name, 64);

    fprintf (stderr,
             "hivex_open: header fields:\n"
             "  file version             %" PRIu32 ".%" PRIu32 "\n"
             "  sequence nos             %" PRIu32 " %" PRIu32 "\n"
             "    (sequences nos should match if hive was synched at shutdown)\n"
             "  last modified            %" PRIu64 "\n"
             "    (Windows filetime, x 100 ns since 1601-01-01)\n"
             "  original file name       %s\n"
             "    (only 32 chars are stored, name is probably truncated)\n"
             "  root offset              0x%x + 0x1000\n"
             "  end of last page         0x%x + 0x1000 (total file size 0x%zx)\n"
             "  checksum                 0x%x (calculated 0x%x)\n",
             major_ver, le32toh (h->hdr->minor_ver),
             le32toh (h->hdr->sequence1), le32toh (h->hdr->sequence2),
             h->last_modified,
             name ? name : "(conversion failed)",
             le32toh (h->hdr->offset),
             le32toh (h->hdr->blocks), h->size,
             le32toh (h->hdr->csum), sum);
    free (name);
  }

  h->rootoffs = le32toh (h->hdr->offset) + 0x1000;
  h->endpages = le32toh (h->hdr->blocks) + 0x1000;

  DEBUG (2, "root offset = 0x%zx", h->rootoffs);

  /* We'll set this flag when we see a block with the root offset (ie.
   * the root block).
   */
  int seen_root_block = 0, bad_root_block = 0;

  /* Collect some stats. */
  size_t pages = 0;           /* Number of hbin pages read. */
  size_t smallest_page = SIZE_MAX, largest_page = 0;
  size_t blocks = 0;          /* Total number of blocks found. */
  size_t smallest_block = SIZE_MAX, largest_block = 0, blocks_bytes = 0;
  size_t used_blocks = 0;     /* Total number of used blocks found. */
  size_t used_size = 0;       /* Total size (bytes) of used blocks. */

  /* Read the pages and blocks.  The aim here is to be robust against
   * corrupt or malicious registries.  So we make sure the loops
   * always make forward progress.  We add the address of each block
   * we read to a hash table so pointers will only reference the start
   * of valid blocks.
   */
  size_t off;
  struct ntreg_hbin_page *page;
  for (off = 0x1000; off < h->size; off += le32toh (page->page_size)) {
    if (off >= h->endpages)
      break;

    page = (struct ntreg_hbin_page *) ((char *) h->addr + off);
    if (page->magic[0] != 'h' ||
        page->magic[1] != 'b' ||
        page->magic[2] != 'i' ||
        page->magic[3] != 'n') {
      SET_ERRNO (ENOTSUP,
                 "%s: trailing garbage at end of file "
                 "(at 0x%zx, after %zu pages)",
                 filename, off, pages);
      goto error;
    }

    size_t page_size = le32toh (page->page_size);
    DEBUG (2, "page at 0x%zx, size %zu", off, page_size);
    pages++;
    if (page_size < smallest_page) smallest_page = page_size;
    if (page_size > largest_page) largest_page = page_size;

    if (page_size <= sizeof (struct ntreg_hbin_page) ||
        (page_size & 0x0fff) != 0) {
      SET_ERRNO (ENOTSUP,
                 "%s: page size %zu at 0x%zx, bad registry",
                 filename, page_size, off);
      goto error;
    }

    /* Read the blocks in this page. */
    size_t blkoff;
    struct ntreg_hbin_block *block;
    size_t seg_len;
    for (blkoff = off + 0x20;
         blkoff < off + page_size;
         blkoff += seg_len) {
      blocks++;

      int is_root = blkoff == h->rootoffs;
      if (is_root)
        seen_root_block = 1;

      block = (struct ntreg_hbin_block *) ((char *) h->addr + blkoff);
      int used;
      seg_len = block_len (h, blkoff, &used);
      if (seg_len <= 4 || (seg_len & 3) != 0) {
        SET_ERRNO (ENOTSUP,
                   "%s: block size %" PRIu32 " at 0x%zx, bad registry",
                   filename, le32toh (block->seg_len), blkoff);
        goto error;
      }

      if (h->msglvl >= 2) {
        unsigned char *id = (unsigned char *) block->id;
        int id0 = id[0], id1 = id[1];

        fprintf (stderr, "%s: %s: "
                 "%s block id %d,%d (%c%c) at 0x%zx size %zu%s\n",
                 "hivex", __func__,
                 used ? "used" : "free",
                 id0, id1,
                 c_isprint (id0) ? id0 : '.',
                 c_isprint (id1) ? id1 : '.',
                 blkoff,
                 seg_len, is_root ? " (root)" : "");
      }

      blocks_bytes += seg_len;
      if (seg_len < smallest_block) smallest_block = seg_len;
      if (seg_len > largest_block) largest_block = seg_len;

      if (is_root && !used)
        bad_root_block = 1;

      if (used) {
        used_blocks++;
        used_size += seg_len;

        /* Root block must be an nk-block. */
        if (is_root && (block->id[0] != 'n' || block->id[1] != 'k'))
          bad_root_block = 1;

        /* Note this blkoff is a valid address. */
        BITMAP_SET (h->bitmap, blkoff);
      }
    }
  }

  if (!seen_root_block) {
    SET_ERRNO (ENOTSUP, "%s: no root block found", filename);
    goto error;
  }

  if (bad_root_block) {
    SET_ERRNO (ENOTSUP, "%s: bad root block (free or not nk)", filename);
    goto error;
  }

  DEBUG (1, "successfully read Windows Registry hive file:\n"
         "  pages:          %zu [sml: %zu, lge: %zu]\n"
         "  blocks:         %zu [sml: %zu, avg: %zu, lge: %zu]\n"
         "  blocks used:    %zu\n"
         "  bytes used:     %zu",
         pages, smallest_page, largest_page,
         blocks, smallest_block, blocks_bytes / blocks, largest_block,
         used_blocks, used_size);

  return h;

 error:;
  int err = errno;
  if (h) {
    free (h->bitmap);
    if (h->addr && h->size && h->addr != MAP_FAILED) {
      if (!h->writable)
        munmap (h->addr, h->size);
      else
        free (h->addr);
    }
    if (h->fd >= 0)
      close (h->fd);
    free (h->filename);
    free (h);
  }
  errno = err;
  return NULL;
}