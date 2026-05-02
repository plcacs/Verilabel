static bool reconstruct_chained_fixup(struct MACH0_(obj_t) * bin) {
	if (!bin->dyld_info) {
		return false;
	}
	if (!bin->nsegs) {
		return false;
	}
	bin->nchained_starts = bin->nsegs;
	bin->chained_starts = RZ_NEWS0(struct rz_dyld_chained_starts_in_segment *, bin->nchained_starts);
	if (!bin->chained_starts) {
		return false;
	}
	size_t wordsize = get_word_size(bin);
	ut8 *p = NULL;
	size_t j, count, skip, bind_size;
	int seg_idx = 0;
	ut64 seg_off = 0;
	bind_size = bin->dyld_info->bind_size;
	if (!bind_size || bind_size < 1) {
		return false;
	}
	if (bin->dyld_info->bind_off > bin->size) {
		return false;
	}
	if (bin->dyld_info->bind_off + bind_size > bin->size) {
		return false;
	}
	ut8 *opcodes = calloc(1, bind_size + 1);
	if (!opcodes) {
		return false;
	}
	if (rz_buf_read_at(bin->b, bin->dyld_info->bind_off, opcodes, bind_size) != bind_size) {
		bprintf("Error: read (dyld_info bind) at 0x%08" PFMT64x "\n", (ut64)(size_t)bin->dyld_info->bind_off);
		RZ_FREE(opcodes);
		return false;
	}
	struct rz_dyld_chained_starts_in_segment *cur_seg = NULL;
	size_t cur_seg_idx = 0;
	ut8 *end;
	bool done = false;
	for (p = opcodes, end = opcodes + bind_size; !done && p < end;) {
		ut8 imm = *p & BIND_IMMEDIATE_MASK, op = *p & BIND_OPCODE_MASK;
		p++;
		switch (op) {
		case BIND_OPCODE_DONE:
			done = true;
			break;
		case BIND_OPCODE_THREADED: {
			switch (imm) {
			case BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB: {
				read_uleb128(&p, end);
				break;
			}
			case BIND_SUBOPCODE_THREADED_APPLY: {
				const size_t ps = 0x1000;
				if (!cur_seg || cur_seg_idx != seg_idx) {
					cur_seg_idx = seg_idx;
					cur_seg = bin->chained_starts[seg_idx];
					if (!cur_seg) {
						cur_seg = RZ_NEW0(struct rz_dyld_chained_starts_in_segment);
						if (!cur_seg) {
							break;
						}
						bin->chained_starts[seg_idx] = cur_seg;
						cur_seg->pointer_format = DYLD_CHAINED_PTR_ARM64E;
						cur_seg->page_size = ps;
						cur_seg->page_count = ((bin->segs[seg_idx].vmsize + (ps - 1)) & ~(ps - 1)) / ps;
						if (cur_seg->page_count > 0) {
							cur_seg->page_start = malloc(sizeof(ut16) * cur_seg->page_count);
							if (!cur_seg->page_start) {
								break;
							}
							memset(cur_seg->page_start, 0xff, sizeof(ut16) * cur_seg->page_count);
						}
					}
				}
				if (cur_seg) {
					ut32 page_index = (ut32)(seg_off / ps);
					size_t maxsize = cur_seg->page_count * sizeof(ut16);
					if (page_index < maxsize) {
						cur_seg->page_start[page_index] = seg_off & 0xfff;
					}
				}
				break;
			}
			default:
				bprintf("Error: Unexpected BIND_OPCODE_THREADED sub-opcode: 0x%x\n", imm);
			}
			break;
		}
		case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
		case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
		case BIND_OPCODE_SET_TYPE_IMM:
			break;
		case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
			read_uleb128(&p, end);
			break;
		case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
			while (*p++ && p < end) {
				/* empty loop */
			}
			break;
		case BIND_OPCODE_SET_ADDEND_SLEB:
			rz_sleb128((const ut8 **)&p, end);
			break;
		case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
			seg_idx = imm;
			if (seg_idx >= bin->nsegs) {
				bprintf("Error: BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB"
					" has unexistent segment %d\n",
					seg_idx);
				RZ_FREE(opcodes);
				return false;
			} else {
				seg_off = read_uleb128(&p, end);
			}
			break;
		case BIND_OPCODE_ADD_ADDR_ULEB:
			seg_off += read_uleb128(&p, end);
			break;
		case BIND_OPCODE_DO_BIND:
			break;
		case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
			seg_off += read_uleb128(&p, end) + wordsize;
			break;
		case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
			seg_off += (ut64)imm * (ut64)wordsize + wordsize;
			break;
		case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
			count = read_uleb128(&p, end);
			skip = read_uleb128(&p, end);
			for (j = 0; j < count; j++) {
				seg_off += skip + wordsize;
			}
			break;
		default:
			bprintf("Error: unknown bind opcode 0x%02x in dyld_info\n", *p);
			RZ_FREE(opcodes);
			return false;
		}
	}
	RZ_FREE(opcodes);

	return true;
}