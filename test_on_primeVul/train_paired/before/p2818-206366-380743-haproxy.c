static void http_manage_server_side_cookies(struct stream *s, struct channel *res)
{
	struct session *sess = s->sess;
	struct http_txn *txn = s->txn;
	struct htx *htx;
	struct http_hdr_ctx ctx;
	struct server *srv;
	char *hdr_beg, *hdr_end;
	char *prev, *att_beg, *att_end, *equal, *val_beg, *val_end, *next;
	int is_cookie2 = 0;

	htx = htxbuf(&res->buf);

	ctx.blk = NULL;
	while (1) {
		int is_first = 1;

		if (!http_find_header(htx, ist("Set-Cookie"), &ctx, 1)) {
			if (!http_find_header(htx, ist("Set-Cookie2"), &ctx, 1))
				break;
			is_cookie2 = 1;
		}

		/* OK, right now we know we have a Set-Cookie* at hdr_beg, and
		 * <prev> points to the colon.
		 */
		txn->flags |= TX_SCK_PRESENT;

		/* Maybe we only wanted to see if there was a Set-Cookie (eg:
		 * check-cache is enabled) and we are not interested in checking
		 * them. Warning, the cookie capture is declared in the frontend.
		 */
		if (s->be->cookie_name == NULL && sess->fe->capture_name == NULL)
			break;

		/* OK so now we know we have to process this response cookie.
		 * The format of the Set-Cookie header is slightly different
		 * from the format of the Cookie header in that it does not
		 * support the comma as a cookie delimiter (thus the header
		 * cannot be folded) because the Expires attribute described in
		 * the original Netscape's spec may contain an unquoted date
		 * with a comma inside. We have to live with this because
		 * many browsers don't support Max-Age and some browsers don't
		 * support quoted strings. However the Set-Cookie2 header is
		 * clean.
		 *
		 * We have to keep multiple pointers in order to support cookie
		 * removal at the beginning, middle or end of header without
		 * corrupting the header (in case of set-cookie2). A special
		 * pointer, <scav> points to the beginning of the set-cookie-av
		 * fields after the first semi-colon. The <next> pointer points
		 * either to the end of line (set-cookie) or next unquoted comma
		 * (set-cookie2). All of these headers are valid :
		 *
		 * hdr_beg                                                  hdr_end
		 * |                                                           |
		 * v                                                           |
		 * NAME1  =  VALUE 1  ; Secure; Path="/"                       |
		 * NAME=VALUE; Secure; Expires=Thu, 01-Jan-1970 00:00:01 GMT   v
		 * NAME = VALUE ; Secure; Expires=Thu, 01-Jan-1970 00:00:01 GMT
		 * NAME1 = VALUE 1 ; Max-Age=0, NAME2=VALUE2; Discard
		 * | |   | | |     | |          |
		 * | |   | | |     | |          +-> next
		 * | |   | | |     | +------------> scav
		 * | |   | | |     +--------------> val_end
		 * | |   | | +--------------------> val_beg
		 * | |   | +----------------------> equal
		 * | |   +------------------------> att_end
		 * | +----------------------------> att_beg
		 * +------------------------------> prev
		 * -------------------------------> hdr_beg
		 */
		hdr_beg = ctx.value.ptr;
		hdr_end = hdr_beg + ctx.value.len;
		for (prev = hdr_beg; prev < hdr_end; prev = next) {

			/* Iterate through all cookies on this line */

			/* find att_beg */
			att_beg = prev;
			if (!is_first)
				att_beg++;
			is_first = 0;

			while (att_beg < hdr_end && HTTP_IS_SPHT(*att_beg))
				att_beg++;

			/* find att_end : this is the first character after the last non
			 * space before the equal. It may be equal to hdr_end.
			 */
			equal = att_end = att_beg;

			while (equal < hdr_end) {
				if (*equal == '=' || *equal == ';' || (is_cookie2 && *equal == ','))
					break;
				if (HTTP_IS_SPHT(*equal++))
					continue;
				att_end = equal;
			}

			/* here, <equal> points to '=', a delimiter or the end. <att_end>
			 * is between <att_beg> and <equal>, both may be identical.
			 */

			/* look for end of cookie if there is an equal sign */
			if (equal < hdr_end && *equal == '=') {
				/* look for the beginning of the value */
				val_beg = equal + 1;
				while (val_beg < hdr_end && HTTP_IS_SPHT(*val_beg))
					val_beg++;

				/* find the end of the value, respecting quotes */
				next = http_find_cookie_value_end(val_beg, hdr_end);

				/* make val_end point to the first white space or delimiter after the value */
				val_end = next;
				while (val_end > val_beg && HTTP_IS_SPHT(*(val_end - 1)))
					val_end--;
			}
			else {
				/* <equal> points to next comma, semi-colon or EOL */
				val_beg = val_end = next = equal;
			}

			if (next < hdr_end) {
				/* Set-Cookie2 supports multiple cookies, and <next> points to
				 * a colon or semi-colon before the end. So skip all attr-value
				 * pairs and look for the next comma. For Set-Cookie, since
				 * commas are permitted in values, skip to the end.
				 */
				if (is_cookie2)
					next = http_find_hdr_value_end(next, hdr_end);
				else
					next = hdr_end;
			}

			/* Now everything is as on the diagram above */

			/* Ignore cookies with no equal sign */
			if (equal == val_end)
				continue;

			/* If there are spaces around the equal sign, we need to
			 * strip them otherwise we'll get trouble for cookie captures,
			 * or even for rewrites. Since this happens extremely rarely,
			 * it does not hurt performance.
			 */
			if (unlikely(att_end != equal || val_beg > equal + 1)) {
				int stripped_before = 0;
				int stripped_after = 0;

				if (att_end != equal) {
					memmove(att_end, equal, hdr_end - equal);
					stripped_before = (att_end - equal);
					equal   += stripped_before;
					val_beg += stripped_before;
				}

				if (val_beg > equal + 1) {
					memmove(equal + 1, val_beg, hdr_end + stripped_before - val_beg);
					stripped_after = (equal + 1) - val_beg;
					val_beg += stripped_after;
					stripped_before += stripped_after;
				}

				val_end      += stripped_before;
				next         += stripped_before;
				hdr_end      += stripped_before;

				htx_change_blk_value_len(htx, ctx.blk, hdr_end - hdr_beg);
				ctx.value.len = hdr_end - hdr_beg;
			}

			/* First, let's see if we want to capture this cookie. We check
			 * that we don't already have a server side cookie, because we
			 * can only capture one. Also as an optimisation, we ignore
			 * cookies shorter than the declared name.
			 */
			if (sess->fe->capture_name != NULL &&
			    txn->srv_cookie == NULL &&
			    (val_end - att_beg >= sess->fe->capture_namelen) &&
			    memcmp(att_beg, sess->fe->capture_name, sess->fe->capture_namelen) == 0) {
				int log_len = val_end - att_beg;
				if ((txn->srv_cookie = pool_alloc(pool_head_capture)) == NULL) {
					ha_alert("HTTP logging : out of memory.\n");
				}
				else {
					if (log_len > sess->fe->capture_len)
						log_len = sess->fe->capture_len;
					memcpy(txn->srv_cookie, att_beg, log_len);
					txn->srv_cookie[log_len] = 0;
				}
			}

			srv = objt_server(s->target);
			/* now check if we need to process it for persistence */
			if (!(s->flags & SF_IGNORE_PRST) &&
			    (att_end - att_beg == s->be->cookie_len) && (s->be->cookie_name != NULL) &&
			    (memcmp(att_beg, s->be->cookie_name, att_end - att_beg) == 0)) {
				/* assume passive cookie by default */
				txn->flags &= ~TX_SCK_MASK;
				txn->flags |= TX_SCK_FOUND;

				/* If the cookie is in insert mode on a known server, we'll delete
				 * this occurrence because we'll insert another one later.
				 * We'll delete it too if the "indirect" option is set and we're in
				 * a direct access.
				 */
				if (s->be->ck_opts & PR_CK_PSV) {
					/* The "preserve" flag was set, we don't want to touch the
					 * server's cookie.
					 */
				}
				else if ((srv && (s->be->ck_opts & PR_CK_INS)) ||
				    ((s->flags & SF_DIRECT) && (s->be->ck_opts & PR_CK_IND))) {
					/* this cookie must be deleted */
					if (prev == hdr_beg && next == hdr_end) {
						/* whole header */
						http_remove_header(htx, &ctx);
						/* note: while both invalid now, <next> and <hdr_end>
						 * are still equal, so the for() will stop as expected.
						 */
					} else {
						/* just remove the value */
						int delta = http_del_hdr_value(hdr_beg, hdr_end, &prev, next);
						next      = prev;
						hdr_end  += delta;
					}
					txn->flags &= ~TX_SCK_MASK;
					txn->flags |= TX_SCK_DELETED;
					/* and go on with next cookie */
				}
				else if (srv && srv->cookie && (s->be->ck_opts & PR_CK_RW)) {
					/* replace bytes val_beg->val_end with the cookie name associated
					 * with this server since we know it.
					 */
					int sliding, delta;

					ctx.value = ist2(val_beg, val_end - val_beg);
				        ctx.lws_before = ctx.lws_after = 0;
					http_replace_header_value(htx, &ctx, ist2(srv->cookie, srv->cklen));
					delta     = srv->cklen - (val_end - val_beg);
					sliding   = (ctx.value.ptr - val_beg);
					hdr_beg  += sliding;
					val_beg  += sliding;
					next     += sliding + delta;
					hdr_end  += sliding + delta;

					txn->flags &= ~TX_SCK_MASK;
					txn->flags |= TX_SCK_REPLACED;
				}
				else if (srv && srv->cookie && (s->be->ck_opts & PR_CK_PFX)) {
					/* insert the cookie name associated with this server
					 * before existing cookie, and insert a delimiter between them..
					 */
					int sliding, delta;
					ctx.value = ist2(val_beg, 0);
				        ctx.lws_before = ctx.lws_after = 0;
					http_replace_header_value(htx, &ctx, ist2(srv->cookie, srv->cklen + 1));
					delta     = srv->cklen + 1;
					sliding   = (ctx.value.ptr - val_beg);
					hdr_beg  += sliding;
					val_beg  += sliding;
					next     += sliding + delta;
					hdr_end  += sliding + delta;

					val_beg[srv->cklen] = COOKIE_DELIM;
					txn->flags &= ~TX_SCK_MASK;
					txn->flags |= TX_SCK_REPLACED;
				}
			}
			/* that's done for this cookie, check the next one on the same
			 * line when next != hdr_end (only if is_cookie2).
			 */
		}
	}
}