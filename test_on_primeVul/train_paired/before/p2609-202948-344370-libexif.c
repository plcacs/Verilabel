exif_data_save_data_entry (ExifData *data, ExifEntry *e,
			   unsigned char **d, unsigned int *ds,
			   unsigned int offset)
{
	unsigned int doff, s;
	unsigned int ts;

	if (!data || !data->priv) 
		return;

	/*
	 * Each entry is 12 bytes long. The memory for the entry has
	 * already been allocated.
	 */
	exif_set_short (*d + 6 + offset + 0,
			data->priv->order, (ExifShort) e->tag);
	exif_set_short (*d + 6 + offset + 2,
			data->priv->order, (ExifShort) e->format);

	if (!(data->priv->options & EXIF_DATA_OPTION_DONT_CHANGE_MAKER_NOTE)) {
		/* If this is the maker note tag, update it. */
		if ((e->tag == EXIF_TAG_MAKER_NOTE) && data->priv->md) {
			/* TODO: this is using the wrong ExifMem to free e->data */
			exif_mem_free (data->priv->mem, e->data);
			e->data = NULL;
			e->size = 0;
			exif_mnote_data_set_offset (data->priv->md, *ds - 6);
			exif_mnote_data_save (data->priv->md, &e->data, &e->size);
			e->components = e->size;
			if (exif_format_get_size (e->format) != 1) {
				/* e->format is taken from input code,
				 * but we need to make sure it is a 1 byte
				 * entity due to the multiplication below. */
				e->format = EXIF_FORMAT_UNDEFINED;
			}
		}
	}

	exif_set_long  (*d + 6 + offset + 4,
			data->priv->order, e->components);

	/*
	 * Size? If bigger than 4 bytes, the actual data is not in
	 * the entry but somewhere else.
	 */
	s = exif_format_get_size (e->format) * e->components;
	if (s > 4) {
		unsigned char *t;
		doff = *ds - 6;
		ts = *ds + s;

		/*
		 * According to the TIFF specification,
		 * the offset must be an even number. If we need to introduce
		 * a padding byte, we set it to 0.
		 */
		if (s & 1)
			ts++;
		t = exif_mem_realloc (data->priv->mem, *d, ts);
		if (!t) {
			EXIF_LOG_NO_MEMORY (data->priv->log, "ExifData", ts);
		  	return;
		}
		*d = t;
		*ds = ts;
		exif_set_long (*d + 6 + offset + 8, data->priv->order, doff);
		if (s & 1) 
			*(*d + *ds - 1) = '\0';

	} else
		doff = offset + 8;

	/* Write the data. Fill unneeded bytes with 0. Do not crash with
	 * e->data is NULL */
	if (e->data) {
		memcpy (*d + 6 + doff, e->data, s);
	} else {
		memset (*d + 6 + doff, 0, s);
	}
	if (s < 4) 
		memset (*d + 6 + doff + s, 0, (4 - s));
}