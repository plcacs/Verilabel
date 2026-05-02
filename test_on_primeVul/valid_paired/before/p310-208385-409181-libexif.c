exif_mnote_data_canon_load (ExifMnoteData *ne,
	const unsigned char *buf, unsigned int buf_size)
{
	ExifMnoteDataCanon *n = (ExifMnoteDataCanon *) ne;
	ExifShort c;
	size_t i, tcount, o, datao;

	if (!n || !buf || !buf_size) {
		exif_log (ne->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifMnoteCanon", "Short MakerNote");
		return;
	}
	datao = 6 + n->offset;
	if (CHECKOVERFLOW(datao, buf_size, 2)) {
		exif_log (ne->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifMnoteCanon", "Short MakerNote");
		return;
	}

	/* Read the number of tags */
	c = exif_get_short (buf + datao, n->order);
	datao += 2;

	/* Remove any old entries */
	exif_mnote_data_canon_clear (n);

	/* Reserve enough space for all the possible MakerNote tags */
	n->entries = exif_mem_alloc (ne->mem, sizeof (MnoteCanonEntry) * c);
	if (!n->entries) {
		EXIF_LOG_NO_MEMORY(ne->log, "ExifMnoteCanon", sizeof (MnoteCanonEntry) * c);
		return;
	}

	/* Parse the entries */
	tcount = 0;
	for (i = c, o = datao; i; --i, o += 12) {
		size_t s;

		memset(&n->entries[tcount], 0, sizeof(MnoteCanonEntry));
		if (CHECKOVERFLOW(o,buf_size,12)) {
			exif_log (ne->log, EXIF_LOG_CODE_CORRUPT_DATA,
				"ExifMnoteCanon", "Short MakerNote");
			break;
		}

		n->entries[tcount].tag        = exif_get_short (buf + o, n->order);
		n->entries[tcount].format     = exif_get_short (buf + o + 2, n->order);
		n->entries[tcount].components = exif_get_long (buf + o + 4, n->order);
		n->entries[tcount].order      = n->order;

		exif_log (ne->log, EXIF_LOG_CODE_DEBUG, "ExifMnoteCanon",
			"Loading entry 0x%x ('%s')...", n->entries[tcount].tag,
			 mnote_canon_tag_get_name (n->entries[tcount].tag));

		/* Check if we overflow the multiplication. Use buf_size as the max size for integer overflow detection,
		 * we will check the buffer sizes closer later. */
		if (	exif_format_get_size (n->entries[tcount].format) &&
			buf_size / exif_format_get_size (n->entries[tcount].format) < n->entries[tcount].components
		) {
			exif_log (ne->log, EXIF_LOG_CODE_CORRUPT_DATA,
				  "ExifMnoteCanon", "Tag size overflow detected (%u * %lu)", exif_format_get_size (n->entries[tcount].format), n->entries[tcount].components);
			continue;
		}

		/*
		 * Size? If bigger than 4 bytes, the actual data is not
		 * in the entry but somewhere else (offset).
		 */
		s = exif_format_get_size (n->entries[tcount].format) * 
								  n->entries[tcount].components;
		n->entries[tcount].size = s;
		if (!s) {
			exif_log (ne->log, EXIF_LOG_CODE_CORRUPT_DATA,
				  "ExifMnoteCanon",
				  "Invalid zero-length tag size");
			continue;

		} else {
			size_t dataofs = o + 8;
			if (s > 4) dataofs = exif_get_long (buf + dataofs, n->order) + 6;

			if (CHECKOVERFLOW(dataofs, buf_size, s)) {
				exif_log (ne->log, EXIF_LOG_CODE_DEBUG,
					"ExifMnoteCanon",
					"Tag data past end of buffer (%u > %u)",
					(unsigned)(dataofs + s), buf_size);
				continue;
			}

			n->entries[tcount].data = exif_mem_alloc (ne->mem, s);
			if (!n->entries[tcount].data) {
				EXIF_LOG_NO_MEMORY(ne->log, "ExifMnoteCanon", s);
				continue;
			}
			memcpy (n->entries[tcount].data, buf + dataofs, s);
		}

		/* Tag was successfully parsed */
		++tcount;
	}
	/* Store the count of successfully parsed tags */
	n->count = tcount;
}