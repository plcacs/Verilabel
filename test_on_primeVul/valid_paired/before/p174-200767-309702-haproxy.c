int h2_make_htx_request(struct http_hdr *list, struct htx *htx, unsigned int *msgf, unsigned long long *body_len)
{
	struct ist phdr_val[H2_PHDR_NUM_ENTRIES];
	uint32_t fields; /* bit mask of H2_PHDR_FND_* */
	uint32_t idx;
	int ck, lck; /* cookie index and last cookie index */
	int phdr;
	int ret;
	int i;
	struct htx_sl *sl = NULL;
	unsigned int sl_flags = 0;
	const char *ctl;

	lck = ck = -1; // no cookie for now
	fields = 0;
	for (idx = 0; list[idx].n.len != 0; idx++) {
		if (!list[idx].n.ptr) {
			/* this is an indexed pseudo-header */
			phdr = list[idx].n.len;
		}
		else {
			/* this can be any type of header */
			/* RFC7540#8.1.2: upper case not allowed in header field names.
			 * #10.3: header names must be valid (i.e. match a token).
			 * For pseudo-headers we check from 2nd char and for other ones
			 * from the first char, because HTTP_IS_TOKEN() also excludes
			 * the colon.
			 */
			phdr = h2_str_to_phdr(list[idx].n);

			for (i = !!phdr; i < list[idx].n.len; i++)
				if ((uint8_t)(list[idx].n.ptr[i] - 'A') < 'Z' - 'A' || !HTTP_IS_TOKEN(list[idx].n.ptr[i]))
					goto fail;
		}

		/* RFC7540#10.3: intermediaries forwarding to HTTP/1 must take care of
		 * rejecting NUL, CR and LF characters.
		 */
		ctl = ist_find_ctl(list[idx].v);
		if (unlikely(ctl) && has_forbidden_char(list[idx].v, ctl))
			goto fail;

		if (phdr > 0 && phdr < H2_PHDR_NUM_ENTRIES) {
			/* insert a pseudo header by its index (in phdr) and value (in value) */
			if (fields & ((1 << phdr) | H2_PHDR_FND_NONE)) {
				if (fields & H2_PHDR_FND_NONE) {
					/* pseudo header field after regular headers */
					goto fail;
				}
				else {
					/* repeated pseudo header field */
					goto fail;
				}
			}
			fields |= 1 << phdr;
			phdr_val[phdr] = list[idx].v;
			continue;
		}
		else if (phdr != 0) {
			/* invalid pseudo header -- should never happen here */
			goto fail;
		}

		/* regular header field in (name,value) */
		if (unlikely(!(fields & H2_PHDR_FND_NONE))) {
			/* no more pseudo-headers, time to build the request line */
			sl = h2_prepare_htx_reqline(fields, phdr_val, htx, msgf);
			if (!sl)
				goto fail;
			fields |= H2_PHDR_FND_NONE;
		}

		if (isteq(list[idx].n, ist("host")))
			fields |= H2_PHDR_FND_HOST;

		if (isteq(list[idx].n, ist("content-length"))) {
			ret = h2_parse_cont_len_header(msgf, &list[idx].v, body_len);
			if (ret < 0)
				goto fail;

			sl_flags |= HTX_SL_F_CLEN;
			if (ret == 0)
				continue; // skip this duplicate
		}

		/* these ones are forbidden in requests (RFC7540#8.1.2.2) */
		if (isteq(list[idx].n, ist("connection")) ||
		    isteq(list[idx].n, ist("proxy-connection")) ||
		    isteq(list[idx].n, ist("keep-alive")) ||
		    isteq(list[idx].n, ist("upgrade")) ||
		    isteq(list[idx].n, ist("transfer-encoding")))
			goto fail;

		if (isteq(list[idx].n, ist("te")) && !isteq(list[idx].v, ist("trailers")))
			goto fail;

		/* cookie requires special processing at the end */
		if (isteq(list[idx].n, ist("cookie"))) {
			list[idx].n.len = -1;

			if (ck < 0)
				ck = idx;
			else
				list[lck].n.len = idx;

			lck = idx;
			continue;
		}

		if (!htx_add_header(htx, list[idx].n, list[idx].v))
			goto fail;
	}

	/* RFC7540#8.1.2.1 mandates to reject response pseudo-headers (:status) */
	if (fields & H2_PHDR_FND_STAT)
		goto fail;

	/* Let's dump the request now if not yet emitted. */
	if (!(fields & H2_PHDR_FND_NONE)) {
		sl = h2_prepare_htx_reqline(fields, phdr_val, htx, msgf);
		if (!sl)
			goto fail;
	}

	if (*msgf & H2_MSGF_BODY_TUNNEL)
		*msgf &= ~(H2_MSGF_BODY|H2_MSGF_BODY_CL);

	if (!(*msgf & H2_MSGF_BODY) || ((*msgf & H2_MSGF_BODY_CL) && *body_len == 0) ||
	    (*msgf & H2_MSGF_BODY_TUNNEL)) {
		/* Request without body or tunnel requested */
		sl_flags |= HTX_SL_F_BODYLESS;
		htx->flags |= HTX_FL_EOM;
	}

	if (*msgf & H2_MSGF_EXT_CONNECT) {
		if (!htx_add_header(htx, ist("upgrade"), phdr_val[H2_PHDR_IDX_PROT]))
			goto fail;
		if (!htx_add_header(htx, ist("connection"), ist("upgrade")))
			goto fail;
		sl_flags |= HTX_SL_F_CONN_UPG;
	}

	/* update the start line with last detected header info */
	sl->flags |= sl_flags;

	/* complete with missing Host if needed */
	if ((fields & (H2_PHDR_FND_HOST|H2_PHDR_FND_AUTH)) == H2_PHDR_FND_AUTH) {
		/* missing Host field, use :authority instead */
		if (!htx_add_header(htx, ist("host"), phdr_val[H2_PHDR_IDX_AUTH]))
			goto fail;
	}

	/* now we may have to build a cookie list. We'll dump the values of all
	 * visited headers.
	 */
	if (ck >= 0) {
		uint32_t fs; // free space
		uint32_t bs; // block size
		uint32_t vl; // value len
		uint32_t tl; // total length
		struct htx_blk *blk;

		blk = htx_add_header(htx, ist("cookie"), list[ck].v);
		if (!blk)
			goto fail;

		tl = list[ck].v.len;
		fs = htx_free_data_space(htx);
		bs = htx_get_blksz(blk);

		/* for each extra cookie, we'll extend the cookie's value and
		 * insert "; " before the new value.
		 */
		fs += tl; // first one is already counted
		while ((ck = list[ck].n.len) >= 0) {
			vl = list[ck].v.len;
			tl += vl + 2;
			if (tl > fs)
				goto fail;

			htx_change_blk_value_len(htx, blk, tl);
			*(char *)(htx_get_blk_ptr(htx, blk) + bs + 0) = ';';
			*(char *)(htx_get_blk_ptr(htx, blk) + bs + 1) = ' ';
			memcpy(htx_get_blk_ptr(htx, blk) + bs + 2, list[ck].v.ptr, vl);
			bs += vl + 2;
		}

	}

	/* now send the end of headers marker */
	if (!htx_add_endof(htx, HTX_BLK_EOH))
		goto fail;

	/* proceed to scheme-based normalization on target-URI */
	if (fields & H2_PHDR_FND_SCHM)
		http_scheme_based_normalize(htx);

	ret = 1;
	return ret;

 fail:
	return -1;
}