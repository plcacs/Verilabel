static void extract_arg(RAnal *anal, RAnalFunction *fcn, RAnalOp *op, const char *reg, const char *sign, char type) {
	st64 ptr = 0;
	char *addr, *esil_buf = NULL;
	const st64 maxstackframe = 1024 * 8; 

	r_return_if_fail (anal && fcn && op && reg);

	size_t i;
	for (i = 0; i < R_ARRAY_SIZE (op->src); i++) {
		if (op->src[i] && op->src[i]->reg && op->src[i]->reg->name) {
			if (!strcmp (reg, op->src[i]->reg->name)) {
				st64 delta = op->src[i]->delta;
				if ((delta > 0 && *sign == '+') || (delta < 0 && *sign == '-')) {
					ptr = R_ABS (op->src[i]->delta);
					break;
				}
			}
		}
	}

	if (!ptr) {
		const char *op_esil = r_strbuf_get (&op->esil);
		if (!op_esil) {
			return;
		}
		esil_buf = strdup (op_esil);
		if (!esil_buf) {
			return;
		}
		r_strf_var (esilexpr, 64, ",%s,%s,", reg, sign);
		char *ptr_end = strstr (esil_buf, esilexpr);
		if (!ptr_end) {
			free (esil_buf);
			return;
		}
		*ptr_end = 0;
		addr = ptr_end;
		while ((addr[0] != '0' || addr[1] != 'x') && addr >= esil_buf + 1 && *addr != ',') {
			addr--;
		}
		if (strncmp (addr, "0x", 2)) {
			//XXX: This is a workaround for inconsistent esil
			if (!op->stackop && op->dst) {
				const char *sp = r_reg_get_name (anal->reg, R_REG_NAME_SP);
				const char *bp = r_reg_get_name (anal->reg, R_REG_NAME_BP);
				const char *rn = op->dst->reg ? op->dst->reg->name : NULL;
				if (rn && ((bp && !strcmp (bp, rn)) || (sp && !strcmp (sp, rn)))) {
					if (anal->verbose) {
						eprintf ("Warning: Analysis didn't fill op->stackop for instruction that alters stack at 0x%" PFMT64x ".\n", op->addr);
					}
					goto beach;
				}
			}
			if (*addr == ',') {
				addr++;
			}
			if (!op->stackop && op->type != R_ANAL_OP_TYPE_PUSH && op->type != R_ANAL_OP_TYPE_POP
				&& op->type != R_ANAL_OP_TYPE_RET && r_str_isnumber (addr)) {
				ptr = (st64)r_num_get (NULL, addr);
				if (ptr && op->src[0] && ptr == op->src[0]->imm) {
					goto beach;
				}
			} else if ((op->stackop == R_ANAL_STACK_SET) || (op->stackop == R_ANAL_STACK_GET)) {
				if (op->ptr % 4) {
					goto beach;
				}
				ptr = R_ABS (op->ptr);
			} else {
				goto beach;
			}
		} else {
			ptr = (st64)r_num_get (NULL, addr);
		}
	}

	if (anal->verbose && (!op->src[0] || !op->dst)) {
		eprintf ("Warning: Analysis didn't fill op->src/dst at 0x%" PFMT64x ".\n", op->addr);
	}

	int rw = (op->direction == R_ANAL_OP_DIR_WRITE) ? R_ANAL_VAR_ACCESS_TYPE_WRITE : R_ANAL_VAR_ACCESS_TYPE_READ;
	if (*sign == '+') {
		const bool isarg = type == R_ANAL_VAR_KIND_SPV ? ptr >= fcn->stack : ptr >= fcn->bp_off;
		const char *pfx = isarg ? ARGPREFIX : VARPREFIX;
		st64 frame_off;
		if (type == R_ANAL_VAR_KIND_SPV) {
			frame_off = ptr - fcn->stack;
		} else {
			frame_off = ptr - fcn->bp_off;
		}
		if (maxstackframe != 0 && (frame_off > maxstackframe || frame_off < -maxstackframe)) {
			goto beach;
		}
		RAnalVar *var = get_stack_var (fcn, frame_off);
		if (var) {
			r_anal_var_set_access (var, reg, op->addr, rw, ptr);
			goto beach;
		}
		char *varname = NULL, *vartype = NULL;
		if (isarg) {
			const char *place = fcn->cc ? r_anal_cc_arg (anal, fcn->cc, ST32_MAX) : NULL;
			bool stack_rev = place ? !strcmp (place, "stack_rev") : false;
			char *fname = r_type_func_guess (anal->sdb_types, fcn->name);
			if (fname) {
				ut64 sum_sz = 0;
				size_t from, to, i;
				if (stack_rev) {
					const size_t cnt = r_type_func_args_count (anal->sdb_types, fname);
					from = cnt ? cnt - 1 : cnt;
					to = fcn->cc ? r_anal_cc_max_arg (anal, fcn->cc) : 0;
				} else {
					from = fcn->cc ? r_anal_cc_max_arg (anal, fcn->cc) : 0;
					to = r_type_func_args_count (anal->sdb_types, fname);
				}
				const int bytes = (fcn->bits ? fcn->bits : anal->bits) / 8;
				for (i = from; stack_rev ? i >= to : i < to; stack_rev ? i-- : i++) {
					char *tp = r_type_func_args_type (anal->sdb_types, fname, i);
					if (!tp) {
						break;
					}
					if (sum_sz == frame_off) {
						vartype = tp;
						varname = strdup (r_type_func_args_name (anal->sdb_types, fname, i));
						break;
					}
					ut64 bit_sz = r_type_get_bitsize (anal->sdb_types, tp);
					sum_sz += bit_sz ? bit_sz / 8 : bytes;
					sum_sz = R_ROUND (sum_sz, bytes);
					free (tp);
				}
				free (fname);
			}
		}
		if (!varname) {
			if (anal->opt.varname_stack) {
				varname = r_str_newf ("%s_%" PFMT64x "h", pfx, R_ABS (frame_off));
			} else {
				varname = r_anal_function_autoname_var (fcn, type, pfx, ptr);
			}
		}
		if (varname) {
#if 0
			if (isarg && frame_off > 48) {
				free (varname);
				goto beach;
			}
#endif
			RAnalVar *var = r_anal_function_set_var (fcn, frame_off, type, vartype, anal->bits / 8, isarg, varname);
			if (var) {
				r_anal_var_set_access (var, reg, op->addr, rw, ptr);
			}
			free (varname);
		}
		free (vartype);
	} else {
		st64 frame_off = -(ptr + fcn->bp_off);
		if (maxstackframe != 0 && (frame_off > maxstackframe || frame_off < -maxstackframe)) {
			goto beach;
		}
		RAnalVar *var = get_stack_var (fcn, frame_off);
		if (var) {
			r_anal_var_set_access (var, reg, op->addr, rw, -ptr);
			goto beach;
		}
		char *varname = anal->opt.varname_stack
			? r_str_newf ("%s_%" PFMT64x "h", VARPREFIX, R_ABS (frame_off))
			: r_anal_function_autoname_var (fcn, type, VARPREFIX, -ptr);
		if (varname) {
			RAnalVar *var = r_anal_function_set_var (fcn, frame_off, type, NULL, anal->bits / 8, false, varname);
			if (var) {
				r_anal_var_set_access (var, reg, op->addr, rw, -ptr);
			}
			free (varname);
		}
	}
beach:
	free (esil_buf);
}