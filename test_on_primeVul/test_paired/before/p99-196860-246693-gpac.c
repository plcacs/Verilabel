GF_Err afra_box_read(GF_Box *s, GF_BitStream *bs)
{
	unsigned int i;
	GF_AdobeFragRandomAccessBox *ptr = (GF_AdobeFragRandomAccessBox *)s;

	ISOM_DECREASE_SIZE(ptr, 9)
	ptr->long_ids = gf_bs_read_int(bs, 1);
	ptr->long_offsets = gf_bs_read_int(bs, 1);
	ptr->global_entries = gf_bs_read_int(bs, 1);
	ptr->reserved = gf_bs_read_int(bs, 5);
	ptr->time_scale = gf_bs_read_u32(bs);

	ptr->entry_count = gf_bs_read_u32(bs);
	if (ptr->size / ( (ptr->long_offsets ? 16 : 12) ) < ptr->entry_count)
		return GF_ISOM_INVALID_FILE;

	for (i=0; i<ptr->entry_count; i++) {
		GF_AfraEntry *ae = gf_malloc(sizeof(GF_AfraEntry));
		if (!ae) return GF_OUT_OF_MEM;

		ISOM_DECREASE_SIZE(ptr, 8)
		ae->time = gf_bs_read_u64(bs);
		if (ptr->long_offsets) {
			ISOM_DECREASE_SIZE(ptr, 8)
			ae->offset = gf_bs_read_u64(bs);
		} else {
			ISOM_DECREASE_SIZE(ptr, 4)
			ae->offset = gf_bs_read_u32(bs);
		}

		gf_list_insert(ptr->local_access_entries, ae, i);
	}

	if (ptr->global_entries) {
		ISOM_DECREASE_SIZE(ptr, 4)
		ptr->global_entry_count = gf_bs_read_u32(bs);
		for (i=0; i<ptr->global_entry_count; i++) {
			GF_GlobalAfraEntry *ae = gf_malloc(sizeof(GF_GlobalAfraEntry));
			if (!ae) return GF_OUT_OF_MEM;
			ISOM_DECREASE_SIZE(ptr, 8)
			ae->time = gf_bs_read_u64(bs);
			if (ptr->long_ids) {
				ISOM_DECREASE_SIZE(ptr, 8)
				ae->segment = gf_bs_read_u32(bs);
				ae->fragment = gf_bs_read_u32(bs);
			} else {
				ISOM_DECREASE_SIZE(ptr, 4)
				ae->segment = gf_bs_read_u16(bs);
				ae->fragment = gf_bs_read_u16(bs);
			}
			if (ptr->long_offsets) {
				ISOM_DECREASE_SIZE(ptr, 16)
				ae->afra_offset = gf_bs_read_u64(bs);
				ae->offset_from_afra = gf_bs_read_u64(bs);
			} else {
				ISOM_DECREASE_SIZE(ptr, 8)
				ae->afra_offset = gf_bs_read_u32(bs);
				ae->offset_from_afra = gf_bs_read_u32(bs);
			}

			gf_list_insert(ptr->global_access_entries, ae, i);
		}
	}

	return GF_OK;
}