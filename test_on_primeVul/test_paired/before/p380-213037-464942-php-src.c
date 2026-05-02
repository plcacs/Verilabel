mbfl_filt_conv_big5_wchar(int c, mbfl_convert_filter *filter)
{
	int k;
	int c1, w, c2;

	switch (filter->status) {
	case 0:
		if (filter->from->no_encoding == mbfl_no_encoding_cp950) {
			c1 = 0x80;
		} else {
			c1 = 0xa0;
		}

		if (c >= 0 && c <= 0x80) {	/* latin */
			CK((*filter->output_function)(c, filter->data));
		} else if (c == 0xff) {
			CK((*filter->output_function)(0xf8f8, filter->data));
		} else if (c > c1 && c < 0xff) {	/* dbcs lead byte */
			filter->status = 1;
			filter->cache = c;
		} else {
			w = c & MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			CK((*filter->output_function)(w, filter->data));
		}
		break;

	case 1:		/* dbcs second byte */
		filter->status = 0;
		c1 = filter->cache;
		if ((c > 0x39 && c < 0x7f) | (c > 0xa0 && c < 0xff)) {
			if (c < 0x7f){
				w = (c1 - 0xa1)*157 + (c - 0x40);
			} else {
				w = (c1 - 0xa1)*157 + (c - 0xa1) + 0x3f;
			}
			if (w >= 0 && w < big5_ucs_table_size) {
				w = big5_ucs_table[w];
			} else {
				w = 0;
			}

			if (filter->from->no_encoding == mbfl_no_encoding_cp950) {
				/* PUA for CP950 */
				if (w <= 0 &&
					(((c1 >= 0xfa && c1 <= 0xfe) || (c1 >= 0x8e && c1 <= 0xa0) ||
					  (c1 >= 0x81 && c1 <= 0x8d) ||(c1 >= 0xc7 && c1 <= 0xc8))
					 && ((c > 0x39 && c < 0x7f) || (c > 0xa0 && c < 0xff))) ||
					((c1 == 0xc6) && (c > 0xa0 && c < 0xff))) {
					c2 = c1 << 8 | c;
					for (k = 0; k < sizeof(cp950_pua_tbl)/(sizeof(unsigned short)*4); k++) {
						if (c2 >= cp950_pua_tbl[k][2] && c2 <= cp950_pua_tbl[k][3]) {
							break;
						}
					}

					if ((cp950_pua_tbl[k][2] & 0xff) == 0x40) {
						w = 157*(c1 - (cp950_pua_tbl[k][2]>>8)) + c - (c >= 0xa1 ? 0x62 : 0x40)
							+ cp950_pua_tbl[k][0];
					} else {
						w = c2 - cp950_pua_tbl[k][2] + cp950_pua_tbl[k][0];
					}
				}
			}

			if (w <= 0) {
				w = (c1 << 8) | c;
				w &= MBFL_WCSPLANE_MASK;
				w |= MBFL_WCSPLANE_BIG5;
			}
			CK((*filter->output_function)(w, filter->data));
		} else if ((c >= 0 && c < 0x21) || c == 0x7f) {		/* CTLs */
			CK((*filter->output_function)(c, filter->data));
		} else {
			w = (c1 << 8) | c;
			w &= MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			CK((*filter->output_function)(w, filter->data));
		}
		break;

	default:
		filter->status = 0;
		break;
	}

	return c;
}