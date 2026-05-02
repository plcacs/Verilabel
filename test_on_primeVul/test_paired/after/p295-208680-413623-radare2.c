R_API void r_core_anal_esil(RCore *core, const char *str, const char *target) {
	bool cfg_anal_strings = r_config_get_i (core->config, "anal.strings");
	bool emu_lazy = r_config_get_i (core->config, "emu.lazy");
	bool gp_fixed = r_config_get_i (core->config, "anal.gpfixed");
	RAnalEsil *ESIL = core->anal->esil;
	ut64 refptr = 0LL;
	char *pcname = NULL;
	RAnalOp op = R_EMPTY;
	ut8 *buf = NULL;
	bool end_address_set = false;
	int iend;
	int minopsize = 4; // XXX this depends on asm->mininstrsize
	bool archIsArm = false;
	ut64 addr = core->offset;
	ut64 start = addr;
	ut64 end = 0LL;
	ut64 cur;
	if (esil_anal_stop || r_cons_is_breaked ()) {
		// faster ^C
		return;
	}

	mycore = core;
	if (!strcmp (str, "?")) {
		eprintf ("Usage: aae[f] [len] [addr] - analyze refs in function, section or len bytes with esil\n");
		eprintf ("  aae $SS @ $S             - analyze the whole section\n");
		eprintf ("  aae $SS str.Hello @ $S   - find references for str.Hellow\n");
		eprintf ("  aaef                     - analyze functions discovered with esil\n");
		return;
	}
#define CHECKREF(x) ((refptr && (x) == refptr) || !refptr)
	if (target) {
		const char *expr = r_str_trim_head_ro (target);
		if (*expr) {
			refptr = ntarget = r_num_math (core->num, expr);
			if (!refptr) {
				ntarget = refptr = addr;
			}
		} else {
			ntarget = UT64_MAX;
			refptr = 0LL;
		}
	} else {
		ntarget = UT64_MAX;
		refptr = 0LL;
	}
	RAnalFunction *fcn = NULL;
	if (!strcmp (str, "f")) {
		fcn = r_anal_get_fcn_in (core->anal, core->offset, 0);
		if (fcn) {
			start = r_anal_function_min_addr (fcn);
			addr = fcn->addr;
			end = r_anal_function_max_addr (fcn);
			end_address_set = true;
		}
	}

	if (!end_address_set) {
		if (str[0] == ' ') {
			end = addr + r_num_math (core->num, str + 1);
		} else {
			RIOMap *map = r_io_map_get_at (core->io, addr);
			if (map) {
				end = r_io_map_end (map);
			} else {
				end = addr + core->blocksize;
			}
		}
	}

	iend = end - start;
	if (iend < 0) {
		return;
	}
	if (iend > MAX_SCAN_SIZE) {
		eprintf ("Warning: Not going to analyze 0x%08"PFMT64x" bytes.\n", (ut64)iend);
		return;
	}
	buf = malloc ((size_t)iend + 2);
	if (!buf) {
		perror ("malloc");
		return;
	}
	esilbreak_last_read = UT64_MAX;
	r_io_read_at (core->io, start, buf, iend + 1);
	if (!ESIL) {
		r_core_cmd0 (core, "aei");
		ESIL = core->anal->esil;
		if (!ESIL) {
			eprintf ("ESIL not initialized\n");
			return;
		}
		r_core_cmd0 (core, "aeim");
		ESIL = core->anal->esil;
	}
	const char *kspname = r_reg_get_name (core->anal->reg, R_REG_NAME_SP);
	if (R_STR_ISEMPTY (kspname)) {
		eprintf ("Error: No =SP defined in the reg profile.\n");
		return;
	}
	char *spname = strdup (kspname);
	EsilBreakCtx ctx = {
		&op,
		fcn,
		spname,
		r_reg_getv (core->anal->reg, spname)
	};
	ESIL->cb.hook_reg_write = &esilbreak_reg_write;
	//this is necessary for the hook to read the id of analop
	ESIL->user = &ctx;
	ESIL->cb.hook_mem_read = &esilbreak_mem_read;
	ESIL->cb.hook_mem_write = &esilbreak_mem_write;

	if (fcn && fcn->reg_save_area) {
		r_reg_setv (core->anal->reg, ctx.spname, ctx.initial_sp - fcn->reg_save_area);
	}
	//eprintf ("Analyzing ESIL refs from 0x%"PFMT64x" - 0x%"PFMT64x"\n", addr, end);
	// TODO: backup/restore register state before/after analysis
	const char *kpcname = r_reg_get_name (core->anal->reg, R_REG_NAME_PC);
	if (!kpcname || !*kpcname) {
		eprintf ("Cannot find program counter register in the current profile.\n");
		return;
	}
	pcname = strdup (kpcname);
	esil_anal_stop = false;
	r_cons_break_push (cccb, core);

	int arch = -1;
	if (!strcmp (core->anal->cur->arch, "arm")) {
		switch (core->anal->cur->bits) {
		case 64: arch = R2_ARCH_ARM64; break;
		case 32: arch = R2_ARCH_ARM32; break;
		case 16: arch = R2_ARCH_THUMB; break;
		}
		archIsArm = true;
	}

	ut64 gp = r_config_get_i (core->config, "anal.gp");
	const char *gp_reg = NULL;
	if (!strcmp (core->anal->cur->arch, "mips")) {
		gp_reg = "gp";
		arch = R2_ARCH_MIPS;
	}

	const char *sn = r_reg_get_name (core->anal->reg, R_REG_NAME_SN);
	if (!sn) {
		eprintf ("Warning: No SN reg alias for current architecture.\n");
	}
	r_reg_arena_push (core->anal->reg);

	IterCtx ictx = { start, end, fcn, NULL };
	size_t i = addr - start;
	size_t i_old = 0;
	do {
		if (esil_anal_stop || r_cons_is_breaked ()) {
			break;
		}
		cur = start + i;
		if (!r_io_is_valid_offset (core->io, cur, 0)) {
			break;
		}
#if 0
		// disabled because it causes some tests to fail
		{
			RPVector *list = r_meta_get_all_in (core->anal, cur, R_META_TYPE_ANY);
			void **it;
			r_pvector_foreach (list, it) {
				RIntervalNode *node = *it;
				RAnalMetaItem *meta = node->data;
				switch (meta->type) {
				case R_META_TYPE_DATA:
				case R_META_TYPE_STRING:
				case R_META_TYPE_FORMAT:
#if 0
					{
						int msz = r_meta_get_size (core->anal, meta->type);
						i += (msz > 0)? msz: minopsize;
					}
					r_pvector_free (list);
					goto loopback;
#elif 0
					{
						int msz = r_meta_get_size (core->anal, meta->type);
						i += (msz > 0)? msz: minopsize;
						i--;
					}
#else
					i += 4;
					goto repeat;
#endif
				default:
					break;
				}
			}
			r_pvector_free (list);
		}
#endif

		/* realign address if needed */
		r_core_seek_arch_bits (core, cur);
		int opalign = core->anal->pcalign;
		if (opalign > 0) {
			cur -= (cur % opalign);
		}

		r_anal_op_fini (&op);
		r_asm_set_pc (core->rasm, cur);
		i_old = i;
		if (i > iend) {
			goto repeat;
		}
		if (!r_anal_op (core->anal, &op, cur, buf + i, iend - i, R_ANAL_OP_MASK_ESIL | R_ANAL_OP_MASK_VAL | R_ANAL_OP_MASK_HINT)) {
			i += minopsize - 1; //   XXX dupe in op.size below
		}
		if (op.type == R_ANAL_OP_TYPE_ILL || op.type == R_ANAL_OP_TYPE_UNK) {
			// i += 2
			r_anal_op_fini (&op);
			goto repeat;
		}
		//we need to check again i because buf+i may goes beyond its boundaries
		//because of i+= minopsize - 1
		if (op.size < 1) {
			i += minopsize - 1;
			goto repeat;
		}
		if (emu_lazy) {
			if (op.type & R_ANAL_OP_TYPE_REP) {
				i += op.size - 1;
				goto repeat;
			}
			switch (op.type & R_ANAL_OP_TYPE_MASK) {
			case R_ANAL_OP_TYPE_JMP:
			case R_ANAL_OP_TYPE_CJMP:
			case R_ANAL_OP_TYPE_CALL:
			case R_ANAL_OP_TYPE_RET:
			case R_ANAL_OP_TYPE_ILL:
			case R_ANAL_OP_TYPE_NOP:
			case R_ANAL_OP_TYPE_UJMP:
			case R_ANAL_OP_TYPE_IO:
			case R_ANAL_OP_TYPE_LEAVE:
			case R_ANAL_OP_TYPE_CRYPTO:
			case R_ANAL_OP_TYPE_CPL:
			case R_ANAL_OP_TYPE_SYNC:
			case R_ANAL_OP_TYPE_SWI:
			case R_ANAL_OP_TYPE_CMP:
			case R_ANAL_OP_TYPE_ACMP:
			case R_ANAL_OP_TYPE_NULL:
			case R_ANAL_OP_TYPE_CSWI:
			case R_ANAL_OP_TYPE_TRAP:
				i += op.size - 1;
				goto repeat;
			//  those require write support
			case R_ANAL_OP_TYPE_PUSH:
			case R_ANAL_OP_TYPE_POP:
				i += op.size - 1;
				goto repeat;
			}
		}
		if (sn && op.type == R_ANAL_OP_TYPE_SWI) {
			r_strf_buffer (64);
			r_flag_space_set (core->flags, R_FLAGS_FS_SYSCALLS);
			int snv = (arch == R2_ARCH_THUMB)? op.val: (int)r_reg_getv (core->anal->reg, sn);
			RSyscallItem *si = r_syscall_get (core->anal->syscall, snv, -1);
			if (si) {
			//	eprintf ("0x%08"PFMT64x" SYSCALL %-4d %s\n", cur, snv, si->name);
				r_flag_set_next (core->flags, r_strf ("syscall.%s", si->name), cur, 1);
			} else {
				//todo were doing less filtering up top because we can't match against 80 on all platforms
				// might get too many of this path now..
			//	eprintf ("0x%08"PFMT64x" SYSCALL %d\n", cur, snv);
				r_flag_set_next (core->flags, r_strf ("syscall.%d", snv), cur, 1);
			}
			r_flag_space_set (core->flags, NULL);
			r_syscall_item_free (si);
		}
		const char *esilstr = R_STRBUF_SAFEGET (&op.esil);
		i += op.size - 1;
		if (R_STR_ISEMPTY (esilstr)) {
			goto repeat;
		}
		r_anal_esil_set_pc (ESIL, cur);
		r_reg_setv (core->anal->reg, pcname, cur + op.size);
		if (gp_fixed && gp_reg) {
			r_reg_setv (core->anal->reg, gp_reg, gp);
		}
		(void)r_anal_esil_parse (ESIL, esilstr);
		// looks like ^C is handled by esil_parse !!!!
		//r_anal_esil_dumpstack (ESIL);
		//r_anal_esil_stack_free (ESIL);
		switch (op.type) {
		case R_ANAL_OP_TYPE_LEA:
			// arm64
			if (core->anal->cur && arch == R2_ARCH_ARM64) {
				if (CHECKREF (ESIL->cur)) {
					r_anal_xrefs_set (core->anal, cur, ESIL->cur, R_ANAL_REF_TYPE_STRING);
				}
			} else if ((target && op.ptr == ntarget) || !target) {
				if (CHECKREF (ESIL->cur)) {
					if (op.ptr && r_io_is_valid_offset (core->io, op.ptr, !core->anal->opt.noncode)) {
						r_anal_xrefs_set (core->anal, cur, op.ptr, R_ANAL_REF_TYPE_STRING);
					} else {
						r_anal_xrefs_set (core->anal, cur, ESIL->cur, R_ANAL_REF_TYPE_STRING);
					}
				}
			}
			if (cfg_anal_strings) {
				add_string_ref (core, op.addr, op.ptr);
			}
			break;
		case R_ANAL_OP_TYPE_ADD:
			/* TODO: test if this is valid for other archs too */
			if (core->anal->cur && archIsArm) {
				/* This code is known to work on Thumb, ARM and ARM64 */
				ut64 dst = ESIL->cur;
				if ((target && dst == ntarget) || !target) {
					if (CHECKREF (dst)) {
						int type = core_type_by_addr (core, dst); // R_ANAL_REF_TYPE_DATA;
						r_anal_xrefs_set (core->anal, cur, dst, type);
					}
				}
				if (cfg_anal_strings) {
					add_string_ref (core, op.addr, dst);
				}
			} else if ((core->anal->bits == 32 && core->anal->cur && arch == R2_ARCH_MIPS)) {
				ut64 dst = ESIL->cur;
				if (!op.src[0] || !op.src[0]->reg || !op.src[0]->reg->name) {
					break;
				}
				if (!strcmp (op.src[0]->reg->name, "sp")) {
					break;
				}
				if (!strcmp (op.src[0]->reg->name, "zero")) {
					break;
				}
				if ((target && dst == ntarget) || !target) {
					if (dst > 0xffff && op.src[1] && (dst & 0xffff) == (op.src[1]->imm & 0xffff) && myvalid (mycore->io, dst)) {
						RFlagItem *f;
						char *str;
						if (CHECKREF (dst) || CHECKREF (cur)) {
							r_anal_xrefs_set (core->anal, cur, dst, R_ANAL_REF_TYPE_DATA);
							if (cfg_anal_strings) {
								add_string_ref (core, op.addr, dst);
							}
							if ((f = r_core_flag_get_by_spaces (core->flags, dst))) {
								r_meta_set_string (core->anal, R_META_TYPE_COMMENT, cur, f->name);
							} else if ((str = is_string_at (mycore, dst, NULL))) {
								char *str2 = r_str_newf ("esilref: '%s'", str);
								// HACK avoid format string inside string used later as format
								// string crashes disasm inside agf under some conditions.
								// https://github.com/radareorg/radare2/issues/6937
								r_str_replace_char (str2, '%', '&');
								r_meta_set_string (core->anal, R_META_TYPE_COMMENT, cur, str2);
								free (str2);
								free (str);
							}
						}
					}
				}
			}
			break;
		case R_ANAL_OP_TYPE_LOAD:
			{
				ut64 dst = esilbreak_last_read;
				if (dst != UT64_MAX && CHECKREF (dst)) {
					if (myvalid (mycore->io, dst)) {
						r_anal_xrefs_set (core->anal, cur, dst, R_ANAL_REF_TYPE_DATA);
						if (cfg_anal_strings) {
							add_string_ref (core, op.addr, dst);
						}
					}
				}
				dst = esilbreak_last_data;
				if (dst != UT64_MAX && CHECKREF (dst)) {
					if (myvalid (mycore->io, dst)) {
						r_anal_xrefs_set (core->anal, cur, dst, R_ANAL_REF_TYPE_DATA);
						if (cfg_anal_strings) {
							add_string_ref (core, op.addr, dst);
						}
					}
				}
			}
			break;
		case R_ANAL_OP_TYPE_JMP:
			{
				ut64 dst = op.jump;
				if (CHECKREF (dst)) {
					if (myvalid (core->io, dst)) {
						r_anal_xrefs_set (core->anal, cur, dst, R_ANAL_REF_TYPE_CODE);
					}
				}
			}
			break;
		case R_ANAL_OP_TYPE_CALL:
			{
				ut64 dst = op.jump;
				if (CHECKREF (dst)) {
					if (myvalid (core->io, dst)) {
						r_anal_xrefs_set (core->anal, cur, dst, R_ANAL_REF_TYPE_CALL);
					}
					ESIL->old = cur + op.size;
					getpcfromstack (core, ESIL);
				}
			}
			break;
		case R_ANAL_OP_TYPE_UJMP:
		case R_ANAL_OP_TYPE_UCALL:
		case R_ANAL_OP_TYPE_ICALL:
		case R_ANAL_OP_TYPE_RCALL:
		case R_ANAL_OP_TYPE_IRCALL:
		case R_ANAL_OP_TYPE_MJMP:
			{
				ut64 dst = core->anal->esil->jump_target;
				if (dst == 0 || dst == UT64_MAX) {
					dst = r_reg_getv (core->anal->reg, pcname);
				}
				if (CHECKREF (dst)) {
					if (myvalid (core->io, dst)) {
						RAnalRefType ref =
							(op.type & R_ANAL_OP_TYPE_MASK) == R_ANAL_OP_TYPE_UCALL
							? R_ANAL_REF_TYPE_CALL
							: R_ANAL_REF_TYPE_CODE;
						r_anal_xrefs_set (core->anal, cur, dst, ref);
						r_core_anal_fcn (core, dst, UT64_MAX, R_ANAL_REF_TYPE_NULL, 1);
// analyze function here
#if 0
						if (op.type == R_ANAL_OP_TYPE_UCALL || op.type == R_ANAL_OP_TYPE_RCALL) {
							eprintf ("0x%08"PFMT64x"  RCALL TO %llx\n", cur, dst);
						}
#endif
					}
				}
			}
			break;
		default:
			break;
		}
		r_anal_esil_stack_free (ESIL);
repeat:
		if (!r_anal_get_block_at (core->anal, cur)) {
			size_t fcn_i;
			for (fcn_i = i_old + 1; fcn_i <= i; fcn_i++) {
				if (r_anal_get_function_at (core->anal, start + fcn_i)) {
					i = fcn_i - 1;
					break;
				}
			}
		}
		if (i >= iend) {
			break;
		}
	} while (get_next_i (&ictx, &i));
	free (pcname);
	free (spname);
	r_list_free (ictx.bbl);
	r_list_free (ictx.path);
	r_list_free (ictx.switch_path);
	free (buf);
	ESIL->cb.hook_mem_read = NULL;
	ESIL->cb.hook_mem_write = NULL;
	ESIL->cb.hook_reg_write = NULL;
	ESIL->user = NULL;
	r_anal_op_fini (&op);
	r_cons_break_pop ();
	// restore register
	r_reg_arena_pop (core->anal->reg);
}