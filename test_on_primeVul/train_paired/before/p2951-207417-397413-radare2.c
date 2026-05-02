R_API int r_egg_lang_parsechar(REgg *egg, char c) {
	REggEmit *e = egg->remit;
	char *ptr, str[64], *tmp_ptr = NULL;
	int i, j;
	if (c == '\n') {
		egg->lang.line++;
		egg->lang.elem_n = 0;
	}
	/* comments */
	if (egg->lang.skipline) {
		if (c != '\n') {
			egg->lang.oc = c;
			return 0;
		}
		egg->lang.skipline = 0;
	}
	if (egg->lang.mode == DATA) {
		return parsedatachar (egg, c);
	}
	if (egg->lang.mode == INLINE) {
		return parseinlinechar (egg, c);
	}
	/* quotes */
	if (egg->lang.quoteline) {
		if (c != egg->lang.quoteline) {
			if (egg->lang.quotelinevar == 1) {
				if (c == '`') {
					egg->lang.elem[egg->lang.elem_n] = 0;
					egg->lang.elem_n = 0;
					tmp_ptr = r_egg_mkvar (egg, str, egg->lang.elem, 0);
					r_egg_printf (egg, "%s", tmp_ptr);
					free (tmp_ptr);
					egg->lang.quotelinevar = 0;
				} else {
					egg->lang.elem[egg->lang.elem_n++] = c;
				}
			} else {
				if (c == '`') {
					egg->lang.elem_n = 0;
					egg->lang.quotelinevar = 1;
				} else {
					r_egg_printf (egg, "%c", c);
				}
			}
			egg->lang.oc = c;
			return 0;
		} else {
			r_egg_printf (egg, "\n");
			egg->lang.quoteline = 0;
		}
	}

	if (egg->lang.commentmode) {
		if (c == '/' && egg->lang.oc == '*') {
			egg->lang.commentmode = 0;
		}
		egg->lang.oc = c;
		return 0;
	} else if (c == '*' && egg->lang.oc == '/') {
		egg->lang.commentmode = 1;
	}
	if (egg->lang.slurp) {
		if (egg->lang.slurp != '"' && c == egg->lang.slurpin) {	// only happend when (...(...)...)
			exit (eprintf (
					"%s:%d Nesting of expressions not yet supported\n",
					egg->lang.file, egg->lang.line));
		}
		if (c == egg->lang.slurp && egg->lang.oc != '\\') {	// close egg->lang.slurp
			egg->lang.elem[egg->lang.elem_n] = '\0';
			if (egg->lang.elem_n > 0) {
				rcc_element (egg, egg->lang.elem);
			} else {
				e->frame (egg, 0);
			}
			egg->lang.elem_n = 0;
			egg->lang.slurp = 0;
		} else {
			egg->lang.elem[egg->lang.elem_n++] = c;
		}
		egg->lang.elem[egg->lang.elem_n] = '\0';
	} else {
		switch (c) {
		case ';':
			rcc_next (egg);
			break;
		case '"':
			egg->lang.slurp = '"';
			break;
		case '(':
			egg->lang.slurpin = '(';
			egg->lang.slurp = ')';
			break;
		case '{':
			if (CTX > 0) {
				// r_egg_printf (egg, " %s:\n", get_frame_label (0));
				if (egg->lang.nested_callname[CTX] && strstr (egg->lang.nested_callname[CTX], "if") &&
				    strstr (egg->lang.elem, "else")) {
					*egg->lang.elem = '\x00';
					egg->lang.elem_n = 0;
					R_FREE (egg->lang.ifelse_table[CTX][egg->lang.nestedi[CTX] - 1])
					egg->lang.ifelse_table[CTX][egg->lang.nestedi[CTX] - 1] =
						r_str_newf ("  __end_%d_%d_%d",
							egg->lang.nfunctions, CTX, egg->lang.nestedi[CTX]);
				}
				r_egg_printf (egg, "  __begin_%d_%d_%d:\n",
					egg->lang.nfunctions, CTX, egg->lang.nestedi[CTX]);	// %s:\n", get_frame_label (0));
			}
			rcc_context (egg, 1);
			break;
		case '}':
			egg->lang.endframe = egg->lang.nested[CTX];
			if (egg->lang.endframe) {
				// XXX: use egg->lang.endframe[context]
				r_egg_printf (egg, "%s", egg->lang.endframe);
				R_FREE (egg->lang.nested[CTX]);
				// R_FREE (egg->lang.endframe);
			}
			if (CTX > 1) {
				if (egg->lang.nested_callname[CTX - 1] && strstr (egg->lang.nested_callname[CTX - 1], "if")) {
					tmp_ptr = r_str_newf ("__ifelse_%d_%d", CTX - 1, egg->lang.nestedi[CTX - 1] - 1);
					e->jmp (egg, tmp_ptr, 0);
					R_FREE (tmp_ptr);	// mem leak
					egg->lang.ifelse_table[CTX - 1][egg->lang.nestedi[CTX - 1] - 1] =
						r_str_newf ("__end_%d_%d_%d",
							egg->lang.nfunctions, CTX - 1, egg->lang.nestedi[CTX - 1] - 1);
				}
				// if (nestede[CTX]) {
				// r_egg_printf (egg, "%s:\n", nestede[CTX]);
				////nestede[CTX] = NULL;
				// } else {
				r_egg_printf (egg, "  __end_%d_%d_%d:\n",
					egg->lang.nfunctions, CTX - 1, egg->lang.nestedi[CTX - 1] - 1);
				// get_end_frame_label (egg));
				// }
			}
			if (CTX > 0) {
				egg->lang.nbrackets++;
			}
			rcc_context (egg, -1);
			if (CTX == 0) {
				r_egg_printf (egg, "\n");
				// snprintf(str, 64, "__end_%d", egg->lang.nfunctions);
				// e->jmp(egg, str, 0);
				// edit this unnessary jmp to bypass tests
				for (i = 0; i < 32; i++) {
					for (j = 0; j < egg->lang.nestedi[i] && j < 32; j++) {
						if (egg->lang.ifelse_table[i][j]) {
							r_egg_printf (egg, "  __ifelse_%d_%d:\n", i, j);
							e->jmp (egg, egg->lang.ifelse_table[i][j], 0);
							R_FREE (egg->lang.ifelse_table[i][j]);
						}
					}
				}
				// r_egg_printf(egg, "  __end_%d:\n\n", egg->lang.nfunctions);
				// edit this unnessary jmp to bypass tests
				egg->lang.nbrackets = 0;
				egg->lang.nfunctions++;
			}
			break;
		case ':':
			if (egg->lang.oc == '\n' || egg->lang.oc == '}') {
				egg->lang.quoteline = '\n';
			} else {
				egg->lang.elem[egg->lang.elem_n++] = c;
			}
			break;
		case '#':
			if (egg->lang.oc == '\n') {
				egg->lang.skipline = 1;
			}
			break;
		case '/':
			if (egg->lang.oc == '/') {
				egg->lang.skipline = 1;
			} else {
				egg->lang.elem[egg->lang.elem_n++] = c;
			}
			break;
		default:
			egg->lang.elem[egg->lang.elem_n++] = c;
		}
		if (egg->lang.slurp) {
			if (egg->lang.elem_n) {
				ptr = egg->lang.elem;
				egg->lang.elem[egg->lang.elem_n] = '\0';
				while (is_space (*ptr)) {
					ptr++;
				}
				rcc_fun (egg, ptr);
			}
			egg->lang.elem_n = 0;
		}
	}
	if (c != '\t' && c != ' ') {
		egg->lang.oc = c;
	}
	return 0;
}