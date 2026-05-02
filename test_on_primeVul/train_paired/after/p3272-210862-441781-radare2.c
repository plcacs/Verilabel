RList *r_bin_ne_get_entrypoints(r_bin_ne_obj_t *bin) {
	RList *entries = r_list_newf (free);
	if (!entries) {
		return NULL;
	}
	RBinAddr *entry;
	RList *segments = r_bin_ne_get_segments (bin);
	if (!segments) {
		r_list_free (entries);
		return NULL;
	}
	if (bin->ne_header->csEntryPoint) {
		entry = R_NEW0 (RBinAddr);
		if (!entry) {
			r_list_free (entries);
			return NULL;
		}
		entry->bits = 16;
		RBinSection *s = r_list_get_n (segments, bin->ne_header->csEntryPoint - 1);
		entry->paddr = bin->ne_header->ipEntryPoint + (s? s->paddr: 0);
		r_list_append (entries, entry);
	}
	int off = 0;
	while (off < bin->ne_header->EntryTableLength) {
		if (bin->entry_table + off + 32 >= r_buf_size (bin->buf)) {
			break;
		}
		ut8 bundle_length = *(ut8 *)(bin->entry_table + off);
		if (!bundle_length) {
			break;
		}
		off++;
		ut8 bundle_type = *(ut8 *)(bin->entry_table + off);
		off++;
		int i;
		for (i = 0; i < bundle_length; i++) {
			entry = R_NEW0 (RBinAddr);
			if (!entry) {
				r_list_free (entries);
				return NULL;
			}
			off++;
			if (!bundle_type) { // Skip
				off--;
				free (entry);
				break;
			} else if (bundle_type == 0xFF) { // Moveable
				off += 2;
				ut8 segnum = *(bin->entry_table + off);
				off++;
				ut16 segoff = *(ut16 *)(bin->entry_table + off);
				if (segnum > 0) {
					entry->paddr = (ut64)bin->segment_entries[segnum - 1].offset * bin->alignment + segoff;
				}
			} else { // Fixed
				entry->paddr = (ut64)bin->segment_entries[bundle_type - 1].offset * bin->alignment + *(ut16 *)(bin->entry_table + off);
			}
			off += 2;
			r_list_append (entries, entry);
		}
	}
	r_list_free (segments);
	bin->entries = entries;
	return entries;
}