static void rebase_buffer(struct MACH0_(obj_t) *obj, ut64 off, RIODesc *fd, ut8 *buf, int count) {
	if (obj->rebasing_buffer) {
		return;
	}
	obj->rebasing_buffer = true;
	ut64 eob = off + count;
	int i = 0;
	for (; i < obj->nsegs; i++) {
		if (!obj->chained_starts[i]) {
			continue;
		}
		ut64 page_size = obj->chained_starts[i]->page_size;
		ut64 start = obj->segs[i].fileoff;
		ut64 end = start + obj->segs[i].filesize;
		if (end >= off && start <= eob) {
			ut64 page_idx = (R_MAX (start, off) - start) / page_size;
			ut64 page_end_idx = (R_MIN (eob, end) - start) / page_size;
			for (; page_idx <= page_end_idx; page_idx++) {
				if (page_idx >= obj->chained_starts[i]->page_count) {
					break;
				}
				ut16 page_start = obj->chained_starts[i]->page_start[page_idx];
				if (page_start == DYLD_CHAINED_PTR_START_NONE) {
					continue;
				}
				ut64 cursor = start + page_idx * page_size + page_start;
				while (cursor < eob && cursor < end) {
					ut8 tmp[8];
					if (r_buf_read_at (obj->b, cursor, tmp, 8) != 8) {
						break;
					}
					ut64 raw_ptr = r_read_le64 (tmp);
					bool is_auth = IS_PTR_AUTH (raw_ptr);
					bool is_bind = IS_PTR_BIND (raw_ptr);
					ut64 ptr_value = raw_ptr;
					ut64 delta;
					if (is_auth && is_bind) {
						struct dyld_chained_ptr_arm64e_auth_bind *p =
								(struct dyld_chained_ptr_arm64e_auth_bind *) &raw_ptr;
						delta = p->next;
					} else if (!is_auth && is_bind) {
						struct dyld_chained_ptr_arm64e_bind *p =
								(struct dyld_chained_ptr_arm64e_bind *) &raw_ptr;
						delta = p->next;
					} else if (is_auth && !is_bind) {
						struct dyld_chained_ptr_arm64e_auth_rebase *p =
								(struct dyld_chained_ptr_arm64e_auth_rebase *) &raw_ptr;
						delta = p->next;
						ptr_value = p->target + obj->baddr;
					} else {
						struct dyld_chained_ptr_arm64e_rebase *p =
								(struct dyld_chained_ptr_arm64e_rebase *) &raw_ptr;
						delta = p->next;
						ptr_value = ((ut64)p->high8 << 56) | p->target;
					}
					ut64 in_buf = cursor - off;
					if (cursor >= off && cursor <= eob - 8) {
						r_write_le64 (&buf[in_buf], ptr_value);
					}
					cursor += delta * 8;
					if (!delta) {
						break;
					}
				}
			}
		}
	}
	obj->rebasing_buffer = false;
}