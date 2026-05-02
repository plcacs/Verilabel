RList *r_bin_le_get_sections(r_bin_le_obj_t *bin) {
	RList *l = r_list_newf ((RListFree)r_bin_section_free);
	if (!l) {
		return NULL;
	}
	LE_image_header *h = bin->header;
	ut32 pages_start_off = h->datapage;
	int i;
	for (i = 0; i < h->objcnt; i++) {
		RBinSection *sec = R_NEW0 (RBinSection);
		if (!sec) {
			return l;
		}
		LE_object_entry *entry = &bin->objtbl[i];
		if  (!entry) {
			free (sec);
			return l;
		}
		sec->name = r_str_newf ("obj.%d", i + 1);
		sec->vsize = entry->virtual_size;
		sec->vaddr = entry->reloc_base_addr;
		sec->add = true;
		if (entry->flags & O_READABLE) {
			sec->perm |= R_PERM_R;
		}
		if (entry->flags & O_WRITABLE) {
			sec->perm |= R_PERM_W;
		}
		if (entry->flags & O_EXECUTABLE) {
			sec->perm |= R_PERM_X;
		}
		if (entry->flags & O_BIG_BIT) {
			sec->bits = R_SYS_BITS_32;
		} else {
			sec->bits = R_SYS_BITS_16;
		}
		sec->is_data = entry->flags & O_RESOURCE || !(sec->perm & R_PERM_X);
		if (!entry->page_tbl_entries) {
			r_list_append (l, sec);
		}
		int j;
		ut32 page_size_sum = 0;
		ut32 next_idx = i < h->objcnt - 1 ? bin->objtbl[i + 1].page_tbl_idx - 1 : UT32_MAX;
		ut32 objmaptbloff = h->objmap + bin->headerOff;
		ut64 objpageentrysz =  bin->is_le ? sizeof (ut32) : sizeof (LE_object_page_entry);
		for (j = 0; j < entry->page_tbl_entries; j++) {
			LE_object_page_entry page;
			RBinSection *s = R_NEW0 (RBinSection);
			if (!s) {
				r_bin_section_free (sec);
				return l;
			}
			s->name = r_str_newf ("%s.page.%d", sec->name, j);
			s->is_data = sec->is_data;

			int cur_idx = entry->page_tbl_idx + j - 1;
			ut64 page_entry_off = objpageentrysz * cur_idx + objmaptbloff;
			r_buf_read_at (bin->buf, page_entry_off, (ut8 *)&page, sizeof (page));
			if (cur_idx < next_idx) { // If not true rest of pages will be zeroes
				if (bin->is_le) {
					// Why is it big endian???
					ut64 offset = r_buf_read_be32_at (bin->buf, page_entry_off) >> 8;
					s->paddr = (offset - 1) * h->pagesize + pages_start_off;
					if (entry->page_tbl_idx + j == h->mpages) {
						page.size = h->pageshift;
					} else {
						page.size = h->pagesize;
					}
				} else if (page.flags == P_ITERATED) {
					ut64 vaddr = sec->vaddr + page_size_sum;
					__create_iter_sections (l, bin, sec, &page, vaddr, j);
					r_bin_section_free (s);
					page_size_sum += h->pagesize;
					continue;
				} else if (page.flags == P_COMPRESSED) {
					// TODO
					R_LOG_WARN ("Compressed page not handled: %s", s->name);
				} else if (page.flags != P_ZEROED) {
					s->paddr = ((ut64)page.offset << h->pageshift) + pages_start_off;
				}
			}
			s->vsize = h->pagesize;
			s->vaddr = sec->vaddr + page_size_sum;
			s->perm = sec->perm;
			s->size = page.size;
			s->add = true;
			s->bits = sec->bits;
			r_list_append (l, s);
			page_size_sum += s->vsize;
		}
		if (entry->page_tbl_entries) {
			r_bin_section_free (sec);
		}
	}
	return l;
}