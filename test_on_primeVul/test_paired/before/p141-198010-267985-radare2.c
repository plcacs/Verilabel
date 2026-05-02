static int string_scan_range(RList *list, RBinFile *bf, int min,
			      const ut64 from, const ut64 to, int type, int raw, RBinSection *section) {
	RBin *bin = bf->rbin;
	ut8 tmp[R_STRING_SCAN_BUFFER_SIZE];
	ut64 str_start, needle = from;
	int count = 0, i, rc, runes;
	int str_type = R_STRING_TYPE_DETECT;

	// if list is null it means its gonna dump
	r_return_val_if_fail (bf, -1);

	if (type == -1) {
		type = R_STRING_TYPE_DETECT;
	}
	if (from == to) {
		return 0;
	}
	if (from > to) {
		eprintf ("Invalid range to find strings 0x%"PFMT64x" .. 0x%"PFMT64x"\n", from, to);
		return -1;
	}
	st64 len = (st64)(to - from);
	if (len < 1 || len > ST32_MAX) {
		eprintf ("String scan range is invalid (%"PFMT64d" bytes)\n", len);
		return -1;
	}
	ut8 *buf = calloc (len, 1);
	if (!buf || !min) {
		free (buf);
		return -1;
	}
	st64 vdelta = 0, pdelta = 0;
	RBinSection *s = NULL;
	bool ascii_only = false;
	PJ *pj = NULL;
	if (bf->strmode == R_MODE_JSON && !list) {
		pj = pj_new ();
		if (pj) {
			pj_a (pj);
		}
	}
	r_buf_read_at (bf->buf, from, buf, len);
	char *charset = r_sys_getenv ("RABIN2_CHARSET");
	if (!R_STR_ISEMPTY (charset)) {
		RCharset *ch = r_charset_new ();
		if (r_charset_use (ch, charset)) {
			int outlen = len * 4;
			ut8 *out = calloc (len, 4);
			if (out) {
				int res = r_charset_encode_str (ch, out, outlen, buf, len);
				int i;
				// TODO unknown chars should be translated to null bytes
				for (i = 0; i < res; i++) {
					if (out[i] == '?') {
						out[i] = 0;
					}
				}
				len = res;
				free (buf);
				buf = out;
			} else {
				eprintf ("Cannot allocate\n");
			}
		} else {
			eprintf ("Invalid value for RABIN2_CHARSET.\n");
		}
		r_charset_free (ch);
	}
	free (charset);
	RConsIsBreaked is_breaked = (bin && bin->consb.is_breaked)? bin->consb.is_breaked: NULL;
	// may oobread
	while (needle < to) {
		if (is_breaked && is_breaked ()) {
			break;
		}
		// smol optimization
		if (needle + 4 < to) {
			ut32 n1 = r_read_le32 (buf + needle - from);
			if (!n1) {
				needle += 4;
				continue;
			}
		}
		rc = r_utf8_decode (buf + needle - from, to - needle, NULL);
		if (!rc) {
			needle++;
			continue;
		}
		bool addr_aligned = !(needle % 4);

		if (type == R_STRING_TYPE_DETECT) {
			char *w = (char *)buf + needle + rc - from;
			if (((to - needle) > 8 + rc)) {
				// TODO: support le and be
				bool is_wide32le = (needle + rc + 2 < to) && (!w[0] && !w[1] && !w[2] && w[3] && !w[4]);
				// reduce false positives
				if (is_wide32le) {
					if (!w[5] && !w[6] && w[7] && w[8]) {
						is_wide32le = false;
					}
				}
				if (!addr_aligned) {
					is_wide32le = false;
				}
				///is_wide32be &= (n1 < 0xff && n11 < 0xff); // false; // n11 < 0xff;
				if (is_wide32le  && addr_aligned) {
					str_type = R_STRING_TYPE_WIDE32; // asume big endian,is there little endian w32?
				} else {
					// bool is_wide = (n1 && n2 && n1 < 0xff && (!n2 || n2 < 0xff));
					bool is_wide = needle + rc + 4 < to && !w[0] && w[1] && !w[2] && w[3] && !w[4];
					str_type = is_wide? R_STRING_TYPE_WIDE: R_STRING_TYPE_ASCII;
				}
			} else {
				if (rc > 1) {
					str_type = R_STRING_TYPE_UTF8; // could be charset if set :?
				} else {
					str_type = R_STRING_TYPE_ASCII;
				}
			}
		} else if (type == R_STRING_TYPE_UTF8) {
			str_type = R_STRING_TYPE_ASCII; // initial assumption
		} else {
			str_type = type;
		}
		runes = 0;
		str_start = needle;

		/* Eat a whole C string */
		for (i = 0; i < sizeof (tmp) - 4 && needle < to; i += rc) {
			RRune r = {0};
			if (str_type == R_STRING_TYPE_WIDE32) {
				rc = r_utf32le_decode (buf + needle - from, to - needle, &r);
				if (rc) {
					rc = 4;
				}
			} else if (str_type == R_STRING_TYPE_WIDE) {
				rc = r_utf16le_decode (buf + needle - from, to - needle, &r);
				if (rc == 1) {
					rc = 2;
				}
			} else {
				rc = r_utf8_decode (buf + needle - from, to - needle, &r);
				if (rc > 1) {
					str_type = R_STRING_TYPE_UTF8;
				}
			}

			/* Invalid sequence detected */
			if (!rc || (ascii_only && r > 0x7f)) {
				needle++;
				break;
			}

			needle += rc;

			if (r_isprint (r) && r != '\\') {
				if (str_type == R_STRING_TYPE_WIDE32) {
					if (r == 0xff) {
						r = 0;
					}
				}
				rc = r_utf8_encode (tmp + i, r);
				runes++;
				/* Print the escape code */
			} else if (r && r < 0x100 && strchr ("\b\v\f\n\r\t\a\033\\", (char)r)) {
				if ((i + 32) < sizeof (tmp) && r < 93) {
					tmp[i + 0] = '\\';
					tmp[i + 1] = "       abtnvfr             e  "
					             "                              "
					             "                              "
					             "  \\"[r];
				} else {
					// string too long
					break;
				}
				rc = 2;
				runes++;
			} else {
				/* \0 marks the end of C-strings */
				break;
			}
		}

		tmp[i++] = '\0';

		if (runes < min && runes >= 2 && str_type == R_STRING_TYPE_ASCII && needle < to) {
			// back up past the \0 to the last char just in case it starts a wide string
			needle -= 2;
		}
		if (runes >= min) {
			// reduce false positives
			int j, num_blocks, *block_list;
			int *freq_list = NULL, expected_ascii, actual_ascii, num_chars;
			if (str_type == R_STRING_TYPE_ASCII) {
				for (j = 0; j < i; j++) {
					char ch = tmp[j];
					if (ch != '\n' && ch != '\r' && ch != '\t') {
						if (!IS_PRINTABLE (tmp[j])) {
							continue;
						}
					}
				}
			}
			switch (str_type) {
			case R_STRING_TYPE_UTF8:
			case R_STRING_TYPE_WIDE:
			case R_STRING_TYPE_WIDE32:
				num_blocks = 0;
				block_list = r_utf_block_list ((const ut8*)tmp, i - 1,
						str_type == R_STRING_TYPE_WIDE? &freq_list: NULL);
				if (block_list) {
					for (j = 0; block_list[j] != -1; j++) {
						num_blocks++;
					}
				}
				if (freq_list) {
					num_chars = 0;
					actual_ascii = 0;
					for (j = 0; freq_list[j] != -1; j++) {
						num_chars += freq_list[j];
						if (!block_list[j]) { // ASCII
							actual_ascii = freq_list[j];
						}
					}
					free (freq_list);
					expected_ascii = num_blocks ? num_chars / num_blocks : 0;
					if (actual_ascii > expected_ascii) {
						ascii_only = true;
						needle = str_start;
						free (block_list);
						continue;
					}
				}
				free (block_list);
				if (num_blocks > R_STRING_MAX_UNI_BLOCKS) {
					needle++;
					continue;
				}
			}
			RBinString *bs = R_NEW0 (RBinString);
			if (!bs) {
				break;
			}
			bs->type = str_type;
			bs->length = runes;
			bs->size = needle - str_start;
			bs->ordinal = count++;
			// TODO: move into adjust_offset
			switch (str_type) {
			case R_STRING_TYPE_WIDE:
				if (str_start - from > 1) {
					const ut8 *p = buf + str_start - 2 - from;
					if (p[0] == 0xff && p[1] == 0xfe) {
						str_start -= 2; // \xff\xfe
					}
				}
				break;
			case R_STRING_TYPE_WIDE32:
				if (str_start - from > 3) {
					const ut8 *p = buf + str_start - 4 - from;
					if (p[0] == 0xff && p[1] == 0xfe) {
						str_start -= 4; // \xff\xfe\x00\x00
					}
				}
				break;
			}
			if (!s) {
				if (section) {
					s = section;
				} else if (bf->o) {
					s = r_bin_get_section_at (bf->o, str_start, false);
				}
				if (s) {
					vdelta = s->vaddr;
					pdelta = s->paddr;
				}
			}
			ut64 baddr = bf->loadaddr && bf->o? bf->o->baddr: bf->loadaddr;
			bs->paddr = str_start + baddr;
			bs->vaddr = str_start - pdelta + vdelta + baddr;
			bs->string = r_str_ndup ((const char *)tmp, i);
			if (list) {
				r_list_append (list, bs);
				if (bf->o) {
					ht_up_insert (bf->o->strings_db, bs->vaddr, bs);
				}
			} else {
				print_string (bf, bs, raw, pj);
				r_bin_string_free (bs);
			}
			if (from == 0 && to == bf->size) {
				/* force lookup section at the next one */
				s = NULL;
			}
		}
		ascii_only = false;
	}
	free (buf);
	if (pj) {
		pj_end (pj);
		if (bin) {
			RIO *io = bin->iob.io;
			if (io) {
				io->cb_printf ("%s", pj_string (pj));
			}
		}
		pj_free (pj);
	}
	return count;
}